package models

import (
	"time"

	"go.mongodb.org/mongo-driver/bson/primitive"
)

// Machine represents a Quiver GUI client tied to a specific user
type Machine struct {
	ID           primitive.ObjectID `bson:"_id,omitempty"    json:"id"`
	UserID       primitive.ObjectID `bson:"user_id"          json:"user_id"`
	MachineUUID  string             `bson:"machine_uuid"     json:"machine_uuid"`
	FriendlyName string             `bson:"friendly_name"    json:"friendly_name"`
	LastSeen     time.Time          `bson:"last_seen"        json:"last_seen"`
	CreatedAt    time.Time          `bson:"created_at"       json:"created_at"`
	UpdatedAt    time.Time          `bson:"updated_at"       json:"updated_at"`
}
