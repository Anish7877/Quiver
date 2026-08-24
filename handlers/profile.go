package handlers

import (
	"context"
	"net/http"
	"strings"
	"time"

	"quiver-backend/config"
	"quiver-backend/middleware"
	"quiver-backend/models"
	"quiver-backend/utils"

	"github.com/cloudinary/cloudinary-go/v2/api/uploader"
	"github.com/gin-gonic/gin"
	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/bson/primitive"
)

// GET /api/profile/me  (protected)
func GetProfile(c *gin.Context) {
	userID, exists := c.Get(middleware.UserIDKey)
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	objID, err := primitive.ObjectIDFromHex(userID.(string))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid user ID in token"})
		return
	}

	collection := config.GetCollection("users")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	var user models.User
	if err := collection.FindOne(ctx, bson.M{"_id": objID}).Decode(&user); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "User not found"})
		return
	}

	// Optionally decrypt token for the GUI to use
	resp := user.ToResponse()
	if user.HubToken != "" {
		decrypted, err := utils.Decrypt(user.HubToken)
		if err == nil {
			resp.HubToken = decrypted
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"user": resp,
	})
}

// PATCH /api/profile/update  (protected)
// Supports: first_name, last_name, username as JSON
// Avatar is handled separately via POST /api/profile/avatar
func UpdateProfile(c *gin.Context) {
	userID, exists := c.Get(middleware.UserIDKey)
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	objID, err := primitive.ObjectIDFromHex(userID.(string))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid user ID in token"})
		return
	}

	var req models.UpdateProfileRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": formatValidationError(err)})
		return
	}

	updateData := bson.M{}

	if req.FirstName != "" {
		updateData["first_name"] = strings.TrimSpace(req.FirstName)
	}
	if req.LastName != "" {
		updateData["last_name"] = strings.TrimSpace(req.LastName)
	}
	if req.Username != "" {
		newUsername := strings.ToLower(strings.TrimSpace(req.Username))

		// Check username uniqueness (exclude current user)
		collection := config.GetCollection("users")
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()

		count, err := collection.CountDocuments(ctx, bson.M{
			"username": newUsername,
			"_id":      bson.M{"$ne": objID}, // exclude self
		})
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Database error"})
			return
		}
		if count > 0 {
			c.JSON(http.StatusConflict, gin.H{"error": "Username already taken"})
			return
		}
		updateData["username"] = newUsername
	}

	if len(updateData) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "No valid fields provided to update"})
		return
	}

	updateData["updated_at"] = time.Now()

	collection := config.GetCollection("users")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	_, err = collection.UpdateOne(
		ctx,
		bson.M{"_id": objID},
		bson.M{"$set": updateData},
	)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update profile"})
		return
	}

	// Return the updated user
	var updatedUser models.User
	if err := collection.FindOne(ctx, bson.M{"_id": objID}).Decode(&updatedUser); err != nil {
		c.JSON(http.StatusOK, gin.H{"message": "Profile updated successfully"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Profile updated successfully",
		"user":    updatedUser.ToResponse(),
	})
}

// POST /api/profile/hub-credentials (protected)
func UpdateHubCredentials(c *gin.Context) {
	userID, exists := c.Get(middleware.UserIDKey)
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	objID, err := primitive.ObjectIDFromHex(userID.(string))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid user ID in token"})
		return
	}

	var req models.HubCredentialsRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": formatValidationError(err)})
		return
	}

	encryptedToken, err := utils.Encrypt(req.HubToken)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to encrypt token"})
		return
	}

	collection := config.GetCollection("users")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	_, err = collection.UpdateOne(
		ctx,
		bson.M{"_id": objID},
		bson.M{"$set": bson.M{
			"hub_username": req.HubUsername,
			"hub_token":    encryptedToken,
			"updated_at":   time.Now(),
		}},
	)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save hub credentials"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Hub credentials saved securely",
	})
}

// POST /api/profile/avatar  (protected)
// Accepts multipart/form-data with field "avatar"
func UpdateAvatar(c *gin.Context) {
	userID, exists := c.Get(middleware.UserIDKey)
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	objID, err := primitive.ObjectIDFromHex(userID.(string))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid user ID in token"})
		return
	}

	// Parse the uploaded file
	file, header, err := c.Request.FormFile("avatar")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Avatar file is required (field name: 'avatar')"})
		return
	}
	defer file.Close()

	// Basic file size check (5MB max)
	if header.Size > 5*1024*1024 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Avatar file must be under 5MB"})
		return
	}

	// Check content type
	contentType := header.Header.Get("Content-Type")
	allowedTypes := map[string]bool{
		"image/jpeg": true,
		"image/png":  true,
		"image/gif":  true,
		"image/webp": true,
	}
	if !allowedTypes[contentType] {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Only JPEG, PNG, GIF, and WebP images are allowed"})
		return
	}

	// Upload to Cloudinary
	uploadCtx, uploadCancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer uploadCancel()

	resp, uploadErr := config.Cloudinary.Upload.Upload(uploadCtx, file, uploader.UploadParams{
		Folder:         "quiver_avatars",
		PublicID:       "avatar_" + objID.Hex(),            // deterministic ID — overwrites previous avatar
		Transformation: "c_fill,w_200,h_200,q_auto,f_auto", // auto-resize to 200x200
	})
	if uploadErr != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to upload avatar"})
		return
	}

	// Update MongoDB
	collection := config.GetCollection("users")
	dbCtx, dbCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer dbCancel()

	_, err = collection.UpdateOne(
		dbCtx,
		bson.M{"_id": objID},
		bson.M{"$set": bson.M{
			"avatar_url": resp.SecureURL,
			"updated_at": time.Now(),
		}},
	)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save avatar URL"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":    "Avatar updated successfully",
		"avatar_url": resp.SecureURL,
	})
}
