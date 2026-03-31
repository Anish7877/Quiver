package handlers

import (
	"context"
	"net/http"
	"strings"
	"time"

	"quiver-backend/config"
	"quiver-backend/models"
	"quiver-backend/utils"

	"github.com/gin-gonic/gin"
	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/bson/primitive"
	"golang.org/x/crypto/bcrypt"
)

// POST /api/auth/signup
func SignUp(c *gin.Context) {
	var req models.SignUpRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": formatValidationError(err)})
		return
	}

	// Normalize inputs
	req.Email = strings.ToLower(strings.TrimSpace(req.Email))
	req.Username = strings.ToLower(strings.TrimSpace(req.Username))

	collection := config.GetCollection("users")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Check if email already exists
	emailCount, err := collection.CountDocuments(ctx, bson.M{"email": req.Email})
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Database error"})
		return
	}
	if emailCount > 0 {
		c.JSON(http.StatusConflict, gin.H{"error": "An account with this email already exists"})
		return
	}

	// Check if username already exists
	userCount, err := collection.CountDocuments(ctx, bson.M{"username": req.Username})
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Database error"})
		return
	}
	if userCount > 0 {
		c.JSON(http.StatusConflict, gin.H{"error": "This username is already taken"})
		return
	}

	// Hash password (bcrypt cost 12 is good balance of security vs speed)
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), 12)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to secure password"})
		return
	}

	now := time.Now()
	user := models.User{
		ID:        primitive.NewObjectID(),
		FirstName: strings.TrimSpace(req.FirstName),
		LastName:  strings.TrimSpace(req.LastName),
		Username:  req.Username,
		Email:     req.Email,
		Password:  string(hashedPassword),
		AvatarURL: "https://res.cloudinary.com/dfedjfltc/image/upload/v1774885739/Untitled_uh8v2x.jpg",
		CreatedAt: now,
		UpdatedAt: now,
	}

	_, err = collection.InsertOne(ctx, user)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to create account"})
		return
	}

	// Generate token immediately so user is logged in after registration
	token, err := utils.GenerateToken(user.ID.Hex(), user.Email)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Account created but failed to generate token"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"message": "Account created successfully",
		"token":   token,
		"user":    user.ToResponse(),
	})
}

// POST /api/auth/signin
func SignIn(c *gin.Context) {
	var req models.SignInRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": formatValidationError(err)})
		return
	}

	identity := strings.ToLower(strings.TrimSpace(req.Identity))

	collection := config.GetCollection("users")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// Find by email OR username
	filter := bson.M{
		"$or": bson.A{
			bson.M{"email": identity},
			bson.M{"username": identity},
		},
	}

	var user models.User
	err := collection.FindOne(ctx, filter).Decode(&user)
	if err != nil {
		// Use a generic message to avoid user enumeration attacks
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
		return
	}

	// Compare password
	if err := bcrypt.CompareHashAndPassword([]byte(user.Password), []byte(req.Password)); err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid credentials"})
		return
	}

	// Generate JWT
	token, err := utils.GenerateToken(user.ID.Hex(), user.Email)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to generate token"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "Signed in successfully",
		"token":   token,
		"user":    user.ToResponse(),
	})
}

// POST /api/auth/logout  (protected)
// JWT is stateless — the client drops the token.
// This endpoint exists for future token blacklisting or audit logging.
func Logout(c *gin.Context) {
	// In a stateless JWT system, logout is handled client-side
	// (the Qt app deletes the stored token from QSettings).
	// This endpoint is here to acknowledge the logout server-side
	// and can be extended with a token blacklist (Redis) if needed.
	c.JSON(http.StatusOK, gin.H{
		"message": "Logged out successfully. Please discard your token.",
	})
}

// formatValidationError provides user-friendly validation messages
func formatValidationError(err error) string {
	return err.Error()
}
