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
	// Web Portal Routes (For desktop OAuth flow)
	// ──────────────────────────────────────────
	web := router.Group("/web")
	{
		web.GET("/auth", handlers.WebAuthPortal)
	}

	
	protected := api.Group("")
	protected.Use(middleware.AuthRequired())
	{
		// Auth
		protected.POST("/auth/logout", handlers.Logout)

		// Profile
		protected.GET("/profile/me", handlers.GetProfile)
		protected.PATCH("/profile/update", handlers.UpdateProfile)
		protected.POST("/profile/hub-credentials", handlers.UpdateHubCredentials)    
		protected.POST("/profile/avatar", handlers.UpdateAvatar)      

		// Configs (Quick Launch)
		protected.POST("/configs", handlers.SaveConfig)
		protected.GET("/configs", handlers.GetConfigs)
		protected.DELETE("/configs", handlers.DeleteAllConfigs)
	}
}
