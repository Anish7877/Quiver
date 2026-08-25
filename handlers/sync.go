package handlers

import (
	"context"
	"net/http"
	"time"

	"quiver-backend/config"
	"quiver-backend/models"

	"github.com/gin-gonic/gin"
	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/bson/primitive"
)

// SyncContainers handles POST /api/sync/containers
func SyncContainers(c *gin.Context) {
	// Require Auth
	userIDStr, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	// Convert string to ObjectID (assumes middleware already validated it's a valid hex)
	userID, err := primitive.ObjectIDFromHex(userIDStr.(string))
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token"})
		return
	}

	machineUUID := c.GetHeader("X-Machine-UUID")
	if machineUUID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Missing X-Machine-UUID header"})
		return
	}

	var req models.SyncContainersRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": formatValidationError(err)})
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	// 1. Verify this MachineUUID belongs to this User
	machinesCollection := config.GetCollection("machines")
	var machine models.Machine
	err = machinesCollection.FindOne(ctx, bson.M{"machine_uuid": machineUUID, "user_id": userID}).Decode(&machine)
	if err != nil {
		c.JSON(http.StatusForbidden, gin.H{"error": "Invalid MachineUUID for this user"})
		return
	}

	// Update last seen
	_, _ = machinesCollection.UpdateOne(ctx, bson.M{"_id": machine.ID}, bson.M{"$set": bson.M{"last_seen": time.Now(), "updated_at": time.Now()}})

	containersCollection := config.GetCollection("containers")

	// 2. Clear out old containers for this machine
	_, err = containersCollection.DeleteMany(ctx, bson.M{"machine_uuid": machineUUID})
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to sync containers"})
		return
	}

	// 3. Insert new ones
	if len(req.Containers) > 0 {
		var docs []interface{}
		for _, container := range req.Containers {
			container.ID = primitive.NewObjectID()
			container.MachineUUID = machineUUID
			container.CreatedAt = time.Now()
			container.UpdatedAt = time.Now()
			docs = append(docs, container)
		}
		_, err = containersCollection.InsertMany(ctx, docs)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to insert containers"})
			return
		}
	}

	c.JSON(http.StatusOK, gin.H{"message": "Containers synced successfully"})
}

// GetDashboard handles GET /api/dashboard
func GetDashboard(c *gin.Context) {
	userIDStr, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	userID, err := primitive.ObjectIDFromHex(userIDStr.(string))
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token"})
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	machinesCollection := config.GetCollection("machines")
	containersCollection := config.GetCollection("containers")

	// Fetch all machines for user
	var machines []models.Machine
	cursor, err := machinesCollection.Find(ctx, bson.M{"user_id": userID})
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to fetch machines"})
		return
	}
	defer cursor.Close(ctx)
	if err = cursor.All(ctx, &machines); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to decode machines"})
		return
	}

	// Fetch containers for each machine
	type MachineDashboard struct {
		Machine    models.Machine     `json:"machine"`
		Containers []models.Container `json:"containers"`
	}

	var dashboard []MachineDashboard
	for _, m := range machines {
		var containers []models.Container
		cCursor, err := containersCollection.Find(ctx, bson.M{"machine_uuid": m.MachineUUID})
		if err == nil {
			_ = cCursor.All(ctx, &containers)
			cCursor.Close(ctx)
		}
		if containers == nil {
			containers = []models.Container{}
		}
		dashboard = append(dashboard, MachineDashboard{
			Machine:    m,
			Containers: containers,
		})
	}

	if dashboard == nil {
		dashboard = []MachineDashboard{}
	}

	c.JSON(http.StatusOK, gin.H{
		"dashboard": dashboard,
	})
}

// RenameMachine handles PATCH /api/machine/rename
func RenameMachine(c *gin.Context) {
	userIDStr, exists := c.Get("userID")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Unauthorized"})
		return
	}

	userID, err := primitive.ObjectIDFromHex(userIDStr.(string))
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid token"})
		return
	}

	var req struct {
		MachineUUID  string `json:"machine_uuid" binding:"required"`
		FriendlyName string `json:"friendly_name" binding:"required,min=1,max=50"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": formatValidationError(err)})
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	machinesCollection := config.GetCollection("machines")
	res, err := machinesCollection.UpdateOne(
		ctx,
		bson.M{"machine_uuid": req.MachineUUID, "user_id": userID},
		bson.M{"$set": bson.M{"friendly_name": req.FriendlyName, "updated_at": time.Now()}},
	)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to update machine name"})
		return
	}
	if res.MatchedCount == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "Machine not found or not owned by you"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Machine renamed successfully"})
}
