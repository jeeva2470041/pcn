import os
from google import genai

MODEL = "gemini-3.5-flash-lite"

if not os.getenv("GEMINI_API_KEY"):
    print("Error: GEMINI_API_KEY is not configured.")
    print("Configure your API key and run the program again.")
    raise SystemExit(1)

client = genai.Client()
chat = client.chats.create(model=MODEL)

print("\n===================================")
print("      GEMINI TERMINAL CHAT")
print("===================================")
print(f"Model: {MODEL}")
print("Type 'reset' to start a new conversation.")
print("Type 'exit' or 'quit' to close.\n")

while True:
    try:
        prompt = input("You: ").strip()
        if not prompt:
            continue

        if prompt.lower() in ("exit", "quit"):
            print("Gemini: See you, bro!")
            break

        if prompt.lower() == "reset":
            chat = client.chats.create(model=MODEL)
            print("Gemini: New conversation started.\n")
            continue

        response = chat.send_message(prompt)
        print("\nGemini:", response.text or "(No text response)")
        print()
    except KeyboardInterrupt:
        print("\nChat ended.")
        break
    except Exception as error:
        print(f"\nAPI error: {error}\n")
