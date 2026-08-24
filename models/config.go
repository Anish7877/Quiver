package models

import (
	"go.mongodb.org/mongo-driver/bson/primitive"
	"time"
)

// ContainerConfig represents a saved container configuration for Quick Launch.
type ContainerConfig struct {
	ID          primitive.ObjectID `bson:"_id,omitempty" json:"id"`
	UserID      primitive.ObjectID `bson:"user_id" json:"user_id"`
	Type        string             `bson:"type" json:"type"` // "container" or "image"
	Image       string             `bson:"image" json:"image"`
	MemoryLimit string             `bson:"memory_limit,omitempty" json:"memory_limit,omitempty"`
	CPUQuota    string             `bson:"cpu_quota,omitempty" json:"cpu_quota,omitempty"`
	Ports       []string           `bson:"ports,omitempty" json:"ports,omitempty"`
	CreatedAt   time.Time          `bson:"created_at" json:"created_at"`
}

// LiveStatus represents the real-time status of a user's containers for Remote Monitoring.
type LiveStatus struct {
	ID           primitive.ObjectID `bson:"_id,omitempty" json:"id"`
	UserID       primitive.ObjectID `bson:"user_id" json:"user_id"`
	ContainerIDs []string           `bson:"container_ids" json:"container_ids"`
	Statuses     []string           `bson:"statuses" json:"statuses"`
	LastUpdated  time.Time          `bson:"last_updated" json:"last_updated"`
}
