package routes

import (
	"quiver-backend/handlers"
	"quiver-backend/middleware"

	"github.com/gin-gonic/gin"
)

func SetupRoutes(router *gin.Engine) {
	api := router.Group("/api")

	// ──────────────────────────────────────────
	// Public Auth Routes (no token required)
	// ──────────────────────────────────────────
	auth := api.Group("/auth")
	{
		auth.POST("/signup", handlers.SignUp)
		auth.POST("/signin", handlers.SignIn)
	}

	// ──────────────────────────────────────────
	// Protected Routes (Bearer token required)
	// ──────────────────────────────────────────
	protected := api.Group("")
	protected.Use(middleware.AuthRequired())
	{
		// Auth
		protected.POST("/auth/logout", handlers.Logout)

		// Profile
		protected.GET("/profile/me", handlers.GetProfile)
		protected.PATCH("/profile/update", handlers.UpdateProfile)    // JSON: first_name, last_name, username
		protected.POST("/profile/avatar", handlers.UpdateAvatar)      // multipart/form-data: avatar file
	}
}
