package models

import (
	"time"

	"go.mongodb.org/mongo-driver/bson/primitive"
)

type User struct {
	ID          primitive.ObjectID `bson:"_id,omitempty"   json:"id"`
	FirstName   string             `bson:"first_name"      json:"first_name"    binding:"required,min=1,max=50"`
	LastName    string             `bson:"last_name"       json:"last_name"     binding:"required,min=1,max=50"`
	Username    string             `bson:"username"        json:"username"      binding:"required,min=3,max=30,alphanum"`
	Email       string             `bson:"email"           json:"email"         binding:"required,email"`
	Password    string             `bson:"password"        json:"-"` // Never returned in JSON
	AvatarURL   string             `bson:"avatar_url"      json:"avatar_url"`
	HubUsername string             `bson:"hub_username"    json:"hub_username"`
	HubToken    string             `bson:"hub_token"       json:"-"`
	CreatedAt   time.Time          `bson:"created_at"      json:"created_at"`
	UpdatedAt   time.Time          `bson:"updated_at"      json:"updated_at"`
}

// SignUpRequest is the payload for registration
type SignUpRequest struct {
	FirstName    string `json:"first_name" binding:"required,min=1,max=50"`
	LastName     string `json:"last_name"  binding:"required,min=1,max=50"`
	Username     string `json:"username"   binding:"required,min=3,max=30,alphanum"`
	Email        string `json:"email"      binding:"required,email"`
	Password     string `json:"password"   binding:"required,min=8"`
	IsGUI        bool   `json:"is_gui" binding:"omitempty"` // if true, generate a machine token
	FriendlyName string `json:"friendly_name" binding:"omitempty"`
}

// SignInRequest supports login with email OR username
type SignInRequest struct {
	Identity     string `json:"identity" binding:"required"` // email or username
	Password     string `json:"password" binding:"required"`
	IsGUI        bool   `json:"is_gui" binding:"omitempty"`       // if true, this is a GUI client
	MachineUUID  string `json:"machine_uuid" binding:"omitempty"` // if empty and IsGUI is true, backend generates a new one
	FriendlyName string `json:"friendly_name" binding:"omitempty"`
}

// UpdateProfileRequest for PATCH /profile
type UpdateProfileRequest struct {
	FirstName string `json:"first_name" binding:"omitempty,min=1,max=50"`
	LastName  string `json:"last_name"  binding:"omitempty,min=1,max=50"`
	Username  string `json:"username"   binding:"omitempty,min=3,max=30,alphanum"`
}

// HubCredentialsRequest for POST /profile/hub-credentials
type HubCredentialsRequest struct {
	HubUsername string `json:"hub_username"`
	HubToken    string `json:"hub_token"`
}

// UserResponse is the safe public view of a user (no password)
type UserResponse struct {
	ID          primitive.ObjectID `json:"id"`
	FirstName   string             `json:"first_name"`
	LastName    string             `json:"last_name"`
	Username    string             `json:"username"`
	Email       string             `json:"email"`
	AvatarURL   string             `json:"avatar_url"`
	HubUsername string             `json:"hub_username"`
	HubToken    string             `json:"hub_token,omitempty"` // populated on demand
	CreatedAt   time.Time          `json:"created_at"`
	UpdatedAt   time.Time          `json:"updated_at"`
}

// ToResponse converts a User to a safe UserResponse
func (u *User) ToResponse() UserResponse {
	return UserResponse{
		ID:          u.ID,
		FirstName:   u.FirstName,
		LastName:    u.LastName,
		Username:    u.Username,
		Email:       u.Email,
		AvatarURL:   u.AvatarURL,
		HubUsername: u.HubUsername,
		CreatedAt:   u.CreatedAt,
		UpdatedAt:   u.UpdatedAt,
	}
}
