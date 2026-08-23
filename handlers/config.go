package handlers

import (
	"context"
	"fmt"
	"net/http"
	"time"

	"quiver-backend/config"
	"quiver-backend/models"

	"github.com/gin-gonic/gin"
	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/bson/primitive"
	"go.mongodb.org/mongo-driver/mongo/options"
)

// SaveConfig saves a new container configuration for the logged-in user and keeps the last 6.
func SaveConfig(c *gin.Context) {
	fmt.Println("--- SaveConfig Debug ---")
	userID, exists := c.Get("userID")
	if !exists {
		fmt.Println("Error: userID not found in context")
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}
	uidStr, ok := userID.(string)
	if !ok {
		fmt.Printf("Error: userID is not a string. Type: %T, Value: %v\n", userID, userID)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Invalid user ID type"})
		return
	}
	uid, err := primitive.ObjectIDFromHex(uidStr)
	if err != nil {
		fmt.Println("Error: ObjectIDFromHex failed:", err)
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid user ID format"})
		return
	}

	var req models.ContainerConfig
	if err := c.ShouldBindJSON(&req); err != nil {
		fmt.Println("Error: ShouldBindJSON failed:", err)
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	req.UserID = uid
	req.CreatedAt = time.Now()

	collection := config.GetCollection("configs")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	_, err = collection.InsertOne(ctx, req)
	if err != nil {
		fmt.Println("Error: InsertOne failed:", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to save configuration", "details": err.Error()})
		return
	}
	fmt.Println("SaveConfig Success!")

	// Keep only the 6 most recent configurations for this user
	opts := options.Find().SetSort(bson.D{{Key: "created_at", Value: -1}}).SetSkip(6)
	cursor, err := collection.Find(ctx, bson.M{"user_id": uid}, opts)
	if err == nil {
		var oldConfigs []models.ContainerConfig
		if err := cursor.All(ctx, &oldConfigs); err == nil {
			for _, oldConfig := range oldConfigs {
				collection.DeleteOne(ctx, bson.M{"_id": oldConfig.ID})
			}
		}
	}

	c.JSON(http.StatusOK, gin.H{"message": "Configuration saved"})
}

// GetConfigs retrieves the saved configurations for the logged-in user.
func GetConfigs(c *gin.Context) {
	fmt.Println("--- GetConfigs Debug ---")
	userID, exists := c.Get("userID")
	if !exists {
		fmt.Println("Error: userID not found in context")
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}
	uidStr, ok := userID.(string)
	if !ok {
		fmt.Printf("Error: userID is not a string. Type: %T, Value: %v\n", userID, userID)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Invalid user ID type"})
		return
	}
	uid, err := primitive.ObjectIDFromHex(uidStr)
	if err != nil {
		fmt.Println("Error: ObjectIDFromHex failed:", err)
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid user ID format"})
		return
	}

	collection := config.GetCollection("configs")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	opts := options.Find().SetSort(bson.D{{Key: "created_at", Value: -1}})
	cursor, err := collection.Find(ctx, bson.M{"user_id": uid}, opts)
	if err != nil {
		fmt.Println("Error: Find failed:", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to retrieve configurations"})
		return
	}
	defer cursor.Close(ctx)

	var configs []models.ContainerConfig
	if err := cursor.All(ctx, &configs); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to parse configurations"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"configs": configs})
}

// DeleteAllConfigs deletes all configurations for the logged-in user.
func DeleteAllConfigs(c *gin.Context) {
	fmt.Println("--- DeleteAllConfigs Debug ---")
	userID, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}
	uidStr, ok := userID.(string)
	if !ok {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Invalid user ID type"})
		return
	}
	uid, err := primitive.ObjectIDFromHex(uidStr)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Invalid user ID format"})
		return
	}

	collection := config.GetCollection("configs")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	_, err = collection.DeleteMany(ctx, bson.M{"user_id": uid})
	if err != nil {
		fmt.Println("Error: DeleteMany failed:", err)
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to delete configurations"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "All configurations deleted"})
}
