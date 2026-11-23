# Universal Dataset Manager

## Overview
Universal Dataset Manager is a simple web‑based application that lets users create, manage and export datasets **without ever opening Excel**.  All data is stored as CSV files which can be opened directly in Microsoft Excel.  The backend is written **pure C++** using:
- **cpp‑httplib** – lightweight HTTP server
- **nlohmann/json** – JSON parsing / serialization
- **C++17 filesystem** – file handling

The frontend is plain HTML/CSS/JavaScript and communicates with the C++ server via a tiny REST API.

## Features
- Pre‑defined datasets: **Student**, **Employee**, **Product**, **Customer**
- **Custom dataset creation** – name, columns, and field types (Text, Number, Email, Date)
- Dynamic form generation based on dataset meta data
- Add, view, search, delete records
- CSV export (download) – files are ready for Excel
- Simple statistics: total datasets / total records
- Beginner‑friendly, heavily commented C++ code

## Folder Structure
```
UniversalDatasetManager/
├─ frontend/
│   ├─ index.html      # UI layout
│   ├─ style.css       # Blue‑and‑white theme, responsive
│   └─ script.js       # JS – calls the REST API
├─ backend/
│   ├─ server.cpp      # C++ HTTP server & CSV logic
│   ├─ httplib.h       # cpp‑httplib single‑header library
│   ├─ json.hpp        # nlohmann/json single‑header library
│   └─ data/           # CSV files (students.csv, …)
├─ compile.bat        # Build script for Windows (MinGW g++)
└─ README.md          # This file
```

## Prerequisites (Windows)
1. **Git Bash / PowerShell** – to run the build script.
2. **MinGW‑w64** (g++) with C++17 support.  Make sure `g++` is on your `PATH`.
3. Internet connection **once** to download the two header‑only libraries.
   - `httplib.h` → https://github.com/yhirose/cpp-httplib
   - `json.hpp` → https://github.com/nlohmann/json/releases
   (The repository already contains placeholder files with instructions; the build will succeed once you replace them with the real headers.)

## Setup & Build
```bat
:: 1. Open a PowerShell / Command Prompt in the project root
cd "d:\collage\3rd semester\problem solving with c++\UniversalDatasetManager"

:: 2. Download the required headers (one‑time step)
::    You can copy the files manually or use curl/wget if you have them.
::    Example using PowerShell's Invoke-WebRequest:
powershell -Command "
    Invoke-WebRequest -Uri https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -OutFile backend\\httplib.h;
    Invoke-WebRequest -Uri https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp -OutFile backend\\json.hpp
"

:: 3. Build the server
compile.bat
```
The script creates **backend\server.exe**.

## Run the Application
```bat
backend\server.exe
```
You should see:
```
Server started at http://localhost:8080
```
Open a browser and navigate to that URL.  The UI loads, shows the sidebar with the four default datasets, and you can start adding records.

## API Summary (used by the frontend)
| Method | URL | Description |
|--------|-----|-------------|
| GET | `/api/datasets` | Returns JSON `{ "datasets": ["students", "employees", ...] }` |
| GET | `/api/dataset/meta?name=students` | Returns column meta `{ "columns": [{"name":"Student ID","type":"Text"}, ...] }` |
| POST | `/api/dataset/create` | Body: `{ "name":"myset", "columns":[{"name":"A","type":"Number"}, …] }` – creates CSV header and meta file |
| POST | `/api/record/add` | Body: `{ "dataset":"students", "record":{"Student ID":"STU002", …} }` |
| GET | `/api/records?dataset=students` | Returns `{ "columns": [...], "records": [[...], ...] }` |
| POST | `/api/record/delete` | Body: `{ "dataset":"students", "index":0 }` – deletes the row at zero‑based index |
| GET | `/api/records/export?dataset=students` | Streams the CSV file with `Content‑Disposition: attachment` |

## Testing the Core Flow
1. **Start the server** (`backend\server.exe`).
2. **Open the UI** (`http://localhost:8080`).
3. Click **Student** in the sidebar → a form appears.
4. Fill the form with a new student (e.g., `STU002, Jane Doe, 102, BCA, 90, jane@example.com`) and press **Add Record**.
5. The table below updates, showing the new row.  The **Total Records** count increments.
6. Use the **Delete** button on a row to remove it – the table refreshes.
7. Click **Export CSV** – the browser downloads `students.csv`.  Open it in Excel to verify the data.
8. Create a **custom dataset** via the **+ New Dataset** button, add a few columns, then add/edit records – they appear under a new CSV file `backend/data/<name>.csv`.

## Known Limitations & Future Extensions
- No XLSX export yet – can be added with a library such as **xlnt** or **libxlsxwriter**.
- No authentication; the server trusts any local client.
- CSV parsing is simple and assumes no line‑breaks inside fields.
- Edit‑record functionality could be added later.

## License
This educational project is released under the MIT License.

---
*Feel free to explore the source – functions are commented and kept straightforward for beginners.*
