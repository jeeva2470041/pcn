# 🤖 Gemini API Terminal Chatbot — Complete Guide

Complete start-to-end setup guide to running the Gemini API in your Ubuntu terminal.

---

## 📋 Table of Contents
1. [Step 1: Install Required System Packages](#step-1-install-required-system-packages)
2. [Step 2: Create Project Folder & Virtual Environment](#step-2-create-project-folder--virtual-environment)
3. [Step 3: Install the Gemini Python SDK](#step-3-install-the-gemini-python-sdk)
4. [Step 4: Get Your Gemini API Key](#step-4-get-your-gemini-api-key)
5. [Step 5: Configure Your API Key in Ubuntu](#step-5-configure-your-api-key-in-ubuntu)
6. [Step 6: Check Available Models](#step-6-check-available-models)
7. [Step 7: Test Your First API Request (`test_gemini.py`)](#step-7-test-your-first-api-request-test_geminipy)
8. [Step 8: Build the Terminal Chatbot (`gemini_chat.py`)](#step-8-build-the-terminal-chatbot-gemini_chatpy)
9. [Step 9: Run Your Chatbot](#step-9-run-your-chatbot)
10. [Step 10: Troubleshooting & Common Errors](#step-10-troubleshooting--common-errors)

---

## Step 1: Install Required System Packages

Open your Ubuntu terminal and run:

```bash
sudo apt update
sudo apt install -y python3 python3-pip python3-venv
```

Verify your installation:
```bash
python3 --version
pip3 --version
```

---

## Step 2: Create Project Folder & Virtual Environment

Create and navigate to the project directory:
```bash
mkdir -p ~/gemini-terminal
cd ~/gemini-terminal
```

Create a Python virtual environment:
```bash
python3 -m venv .venv
```

Activate the virtual environment:
```bash
source .venv/bin/activate
```
*(Your terminal prompt will now show `(.venv)`).*

> [!NOTE]
> Whenever you open a fresh terminal in the future, navigate and reactivate the virtual environment:
> ```bash
> cd ~/gemini-terminal
> source .venv/bin/activate
> ```

---

## Step 3: Install the Gemini Python SDK

Upgrade `pip` and install the official Google GenAI package:

```bash
python -m pip install --upgrade pip
python -m pip install google-genai
```

Verify the installation:
```bash
python -c "from google import genai; print('Gemini SDK installed successfully')"
```

---

## Step 4: Get Your Gemini API Key

1. Go to [Google AI Studio — API keys](https://aistudio.google.com/apikey).
2. Sign in with your Google account.
3. Click **Create API key** (or choose an existing one).
4. Copy your key.

> [!WARNING]
> Never share your API key, commit it to GitHub, or leave it in public files.

---

## Step 5: Configure Your API Key in Ubuntu

Paste your key into the terminal securely (characters remain hidden while typing):

```bash
read -rsp "Paste your Gemini API key: " GEMINI_API_KEY && echo export GEMINI_API_KEY
```

Verify the key is configured without exposing it on screen:
```bash
if [ -n "$GEMINI_API_KEY" ]; then
    echo "✅ Gemini API key is configured"
else
    echo "❌ API key is missing"
fi
```

The Google GenAI client automatically reads `GEMINI_API_KEY` from your environment.

---

## Step 6: Check Available Models

Check which models your project key has access to by running [list_models.py](list_models.py):

```bash
python list_models.py
```

*(Look for models like `gemini-3.5-flash-lite` or `gemini-3.8-flash` in the output list).*

---

## Step 7: Test Your First API Request (`test_gemini.py`)

Create or review the test file:
```bash
nano test_gemini.py
```

*(See [test_gemini.py](test_gemini.py) for the complete code).*

Run the test:
```bash
python test_gemini.py
```

Expected output:
```text
Gemini response:
Hello! How can I help you today?
```

---

## Step 8: Build the Terminal Chatbot (`gemini_chat.py`)

Create or review the chatbot file:
```bash
nano gemini_chat.py
```

*(See [gemini_chat.py](gemini_chat.py) for the complete code).*

Features included:
- Preserves conversation history/context using `client.chats.create()`.
- Type `reset` to clear chat history and start fresh.
- Type `exit` or `quit` to exit safely.
- Graceful exception handling for connection errors.

---

## Step 9: Run Your Chatbot

Ensure the virtual environment is activated and your API key is set:

```bash
cd ~/gemini-terminal
source .venv/bin/activate
python gemini_chat.py
```

Example interaction:
```text
===================================
      GEMINI TERMINAL CHAT
===================================
Model: gemini-3.5-flash-lite
Type 'reset' to start a new conversation.
Type 'exit' or 'quit' to close.

You: Explain Python functions.
Gemini: A Python function is a reusable block of code...

You: Give me an example.
Gemini: Here is a simple example...

You: exit
Gemini: See you, bro!
```

---

## Step 10: Troubleshooting & Common Errors

| Error | Cause | What to do |
|---|---|---|
| `ModuleNotFoundError` | Virtual environment inactive or package not installed | Run `source .venv/bin/activate` and `pip install google-genai` |
| `API key is not configured` | Environment variable not exported | Run `export GEMINI_API_KEY="your_key"` in the same terminal |
| `400` or `404 NOT_FOUND` | Model name misspelled or deprecated (e.g. `gemini-2.5-flash`) | Change model to `gemini-3.5-flash-lite` or check with `python list_models.py` |
| `401` or `403` | Invalid API key or permission denied | Re-copy key from Google AI Studio and re-export |
| `429 RESOURCE_EXHAUSTED` | Rate limit (RPM) or daily quota (RPD) reached | Wait 1 min for RPM, or use an API key from a different GCP project |
| `insufficient_quota` | Project quota pool exhausted | Switch to a model with higher free quota like `gemini-3.5-flash-lite` (~500 req/day) |
