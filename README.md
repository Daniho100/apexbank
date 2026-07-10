# Banking Application MVP

This is a production-quality Banking Application MVP built around an immutable financial ledger and a secure, role-based architecture.

## Repository Structure

- `backend/`: C++20 backend built with the Drogon Framework.
- `frontend/`: React.js, TypeScript, React Query, and TailwindCSS client dashboard.
- `desktop/`: Electron wrapper for packaging the React application as a cross-platform desktop application.
- `docker-compose.yml`: Database configurations (PostgreSQL and Redis).

## Ports and Connectivity

To prevent conflict with other projects, the following ports are configured:
- **PostgreSQL**: `5433`
- **Redis**: `6380`
- **Backend HTTP/WebSocket**: `8080`
- **Frontend Dev Server**: `5173`

## Running the Application

1. **Start Databases**:
   ```bash
   docker compose up -d
   ```
2. **Build and Run Backend**:
   Detailed compilation and execution guides are provided in `backend/README.md`.
3. **Run Frontend**:
   ```bash
   cd frontend
   npm install
   npm run dev
   ```
4. **Run Desktop Shell**:
   ```bash
   cd desktop
   npm install
   npm start
   ```
