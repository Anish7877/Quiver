package config

import (
	"context"
	"log"
	"os"
	"time"

	"github.com/cloudinary/cloudinary-go/v2"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"
)

var DB *mongo.Client
var Cloudinary *cloudinary.Cloudinary

func ConnectDatabase() {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	mongoURI := os.Getenv("MONGO_URI")
	if mongoURI == "" {
		log.Fatal("MONGO_URI environment variable not set")
	}

	client, err := mongo.Connect(ctx, options.Client().ApplyURI(mongoURI))
	if err != nil {
		log.Fatal("Failed to connect to MongoDB: ", err)
	}

	err = client.Ping(ctx, nil)
	if err != nil {
		log.Fatal("Failed to ping MongoDB: ", err)
	}

	log.Println("✅ Successfully connected to MongoDB!")
	DB = client
}

func ConnectCloudinary() {
	cloudinaryURL := os.Getenv("CLOUDINARY_URL")
	if cloudinaryURL == "" {
		log.Fatal("CLOUDINARY_URL environment variable not set")
	}

	cld, err := cloudinary.NewFromURL(cloudinaryURL)
	if err != nil {
		log.Fatal("Failed to initialize Cloudinary: ", err)
	}

	log.Println("✅ Successfully connected to Cloudinary!")
	Cloudinary = cld
}

// GetCollection returns a MongoDB collection by name
func GetCollection(collectionName string) *mongo.Collection {
	dbName := os.Getenv("DB_NAME")
	if dbName == "" {
		dbName = "quiver_db"
	}
	return DB.Database(dbName).Collection(collectionName)
}
