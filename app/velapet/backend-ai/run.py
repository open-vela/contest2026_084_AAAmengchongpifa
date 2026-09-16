"""Run from this directory: python run.py. Loads .env before importing the app."""
from pathlib import Path
import os

from dotenv import load_dotenv
import uvicorn

if __name__ == "__main__":
    os.chdir(Path(__file__).resolve().parent)
    load_dotenv()
    from app.config import Settings
    settings = Settings.from_env()
    uvicorn.run("app.main:app", host=settings.host, port=settings.port, workers=1)
