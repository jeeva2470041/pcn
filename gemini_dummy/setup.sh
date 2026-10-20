#!/bin/bash
# ==========================================
# Gemini Terminal Setup Script for Ubuntu
# ==========================================

echo "Step 1: Installing system packages..."
sudo apt update
sudo apt install -y python3 python3-pip python3-venv

echo "Step 2: Setting up virtual environment..."
mkdir -p ~/gemini-terminal
cd ~/gemini-terminal
python3 -m venv .venv
source .venv/bin/activate

echo "Step 3: Installing Google GenAI SDK..."
python -m pip install --upgrade pip
python -m pip install google-genai

echo "Step 4: Verification..."
python -c "from google import genai; print('✅ Gemini SDK installed successfully')"

echo ""
echo "Now configure your API key by running:"
echo 'read -rsp "Paste your Gemini API key: " GEMINI_API_KEY && echo export GEMINI_API_KEY'
