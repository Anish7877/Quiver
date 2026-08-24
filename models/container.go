package models

import (
	"time"

	"go.mongodb.org/mongo-driver/bson/primitive"
)

// Container represents a synced Quiver container
type Container struct {
	ID          primitive.ObjectID `bson:"_id,omitempty"    json:"id"`
	MachineUUID string             `bson:"machine_uuid"     json:"machine_uuid"`
	ContainerID string             `bson:"container_id"     json:"container_id"`
	Name        string             `bson:"name"             json:"name"`
	Status      string             `bson:"status"           json:"status"`
	Image       string             `bson:"image"            json:"image"`
	Ports       string             `bson:"ports"            json:"ports"`
	CreatedAt   time.Time          `bson:"created_at"       json:"created_at"`
	UpdatedAt   time.Time          `bson:"updated_at"       json:"updated_at"`
}

// SyncContainersRequest is the payload from the GUI
type SyncContainersRequest struct {
	Containers []Container `json:"containers" binding:"required"`
}
