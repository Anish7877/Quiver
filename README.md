# Quiver Backend — Go + MongoDB

Secure REST API for the Quiver Qt6 desktop app.

## Project Structure

```
BackendAuth/
├── main.go                  # Entry point
├── .env                     # Environment variables (never commit this)
├── go.mod
├── config/
│   └── setup.go             # MongoDB + Cloudinary connections
├── models/
│   └── user.go              # User model + request/response types
├── utils/
│   └── jwt.go               # JWT generation + validation
├── middleware/
│   └── auth.go              # JWT auth middleware
├── handlers/
│   ├── auth.go              # SignUp, SignIn, Logout
│   └── profile.go           # GetProfile, UpdateProfile, UpdateAvatar
└── routes/
    └── routes.go            # All route definitions
```

## Setup

### 1. Install Go dependencies

```bash
cd BackendAuth
go mod tidy
```

### 2. Configure .env

Your `.env` is already set up. For production, change `JWT_SECRET` to a long random string:

```
PORT=8080
MONGO_URI=mongodb+srv://...
DB_NAME=quiver_db
JWT_SECRET=change_this_to_a_very_long_random_secret_64_chars_minimum
CLOUDINARY_URL=cloudinary://...
```

### 3. Run the server

```bash
go run main.go
```

You should see:
```
✅ Successfully connected to MongoDB!
✅ Successfully connected to Cloudinary!
🚀 Quiver backend running on http://localhost:8080
```

---

## API Reference

### Base URL: `http://localhost:8080/api`

---

### Public Endpoints

#### `POST /auth/signup`
Register a new account. Returns token + user.

```json
// Request body
{
  "first_name": "Bhavish",
  "last_name": "Pushkarna",
  "username": "bhavish123",
  "email": "bhavish@quiver.dev",
  "password": "SecurePass@123"
}

// Response 201
{
  "message": "Account created successfully",
  "token": "eyJhbGci...",
  "user": { "id": "...", "first_name": "Bhavish", ... }
}
```

#### `POST /auth/signin`
Login with email OR username. Returns token + user.

```json
// Request body (use email or username in "identity")
{
  "identity": "bhavish@quiver.dev",
  "password": "SecurePass@123"
}

// Response 200
{
  "message": "Signed in successfully",
  "token": "eyJhbGci...",
  "user": { ... }
}
```

---

### Protected Endpoints
All require `Authorization: Bearer <token>` header.

#### `POST /auth/logout`
Acknowledges logout. The Qt app should delete the stored token from QSettings.

#### `GET /profile/me`
Returns the current user's profile.

#### `PATCH /profile/update`
Update name and/or username (JSON body).

```json
{
  "first_name": "New Name",
  "last_name": "New Last",
  "username": "newusername"
}
```

#### `POST /profile/avatar`
Upload avatar image. Use `multipart/form-data` with field `avatar`.
- Max size: 5MB
- Allowed types: JPEG, PNG, GIF, WebP
- Auto-resized to 200x200 by Cloudinary

---

## Testing in Postman

1. Import `QuiverAPI.postman_collection.json` into Postman
2. Run **Sign Up** — token auto-saves to collection variable `{{token}}`
3. All protected requests use `{{token}}` automatically
4. Test error cases at the bottom of the collection

---

## Qt6 Integration (AuthManager.cpp)

Replace the mock `login()` function with a real HTTP call:

```cpp
// In AuthManager.cpp — replace the simulated login with:
void AuthManager::login(const QString& identity, const QString& password) {
    QNetworkRequest request(QUrl(pimpl_->api_base_url_ + "/signin"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["identity"] = identity;
    body["password"] = password;

    auto* reply = pimpl_->network_.post(request, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        auto doc = QJsonDocument::fromJson(reply->readAll());
        
        if (reply->error() != QNetworkReply::NoError) {
            emit login_failed(doc["error"].toString());
            return;
        }

        QSettings settings("QuiverApp", "Quiver");
        settings.setValue("jwt_token", doc["token"].toString());
        
        auto user = doc["user"].toObject();
        settings.setValue("username", "@" + user["username"].toString());
        settings.setValue("full_name", user["first_name"].toString() + " " + user["last_name"].toString());
        settings.setValue("avatar_url", user["avatar_url"].toString());

        emit login_success();
    });
}
```

Change the API base URL in `AuthManager.cpp`:
```cpp
QString api_base_url_ { "http://localhost:8080/api/auth" };
```

---

## Security Notes

- Passwords are hashed with bcrypt (cost 12)
- JWT tokens expire after 72 hours
- JWT secret is loaded from env — never hardcoded
- Email/username enumeration is prevented (generic "Invalid credentials" message)
- All protected routes require valid Bearer token
- Avatar uploads are validated (type + size) before hitting Cloudinary
- Username uniqueness is checked excluding current user on update
