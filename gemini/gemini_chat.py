import os
import sys
import getpass
from google import genai
from google.genai import errors

DEFAULT_MODEL = "gemini-3.5-flash-lite"

def get_api_key():
    key = os.getenv("GEMINI_API_KEY", "").strip()
    if not key:
        print("\n⚠️  GEMINI_API_KEY environment variable is not set.")
        key = getpass.getpass("Paste your Gemini API key (invisible while typing): ").strip()
        if not key:
            print("❌ No API key provided. Exiting.")
            sys.exit(1)
        os.environ["GEMINI_API_KEY"] = key
        print("✅ Key accepted for this session!\n")
    return key

def main():
    api_key = get_api_key()
    current_model = DEFAULT_MODEL

    try:
        client = genai.Client(api_key=api_key)
        chat = client.chats.create(model=current_model)
    except Exception as e:
        print(f"❌ Initialization error: {e}")
        sys.exit(1)

    print("=" * 60)
    print("🤖 Gemini Terminal Chatbot")
    print(f"📌 Active Model: {current_model}")
    print("=" * 60)
    print("Commands:")
    print("  • exit / quit     : Exit the chatbot")
    print("  • reset           : Clear conversation memory")
    print("  • model [name]    : Switch model (e.g., model gemini-3.8-flash)")
    print("  • help            : Show commands")
    print("=" * 60 + "\n")

    while True:
        try:
            prompt = input("You: ").strip()

            if not prompt:
                continue

            cmd_lower = prompt.lower()

            if cmd_lower in ("exit", "quit"):
                print("\nGemini: See you later, bro! 👋\n")
                break

            if cmd_lower == "reset":
                chat = client.chats.create(model=current_model)
                print(f"\n🔄 Memory cleared! Fresh session with {current_model}.\n")
                continue

            if cmd_lower == "help":
                print("\nCommands: 'exit', 'quit', 'reset', 'model [name]'\n")
                continue

            if cmd_lower.startswith("model "):
                new_model = prompt.split(" ", 1)[1].strip()
                if new_model:
                    try:
                        print(f"Switching model to {new_model}...")
                        test_chat = client.chats.create(model=new_model)
                        chat = test_chat
                        current_model = new_model
                        print(f"✅ Successfully switched to {current_model}!\n")
                    except Exception as err:
                        print(f"❌ Failed to switch to {new_model}: {err}\n")
                continue

            response = chat.send_message(prompt)
            print(f"\nGemini:\n{response.text}\n")

        except errors.ClientError as e:
            err_str = str(e)
            if "404" in err_str:
                print(f"\n❌ Model '{current_model}' not found or deprecated.")
                print("💡 Try: model gemini-3.5-flash-lite or model gemini-3.8-flash\n")
            elif "429" in err_str:
                print("\n⚠️ Quota / Rate limit reached for this project!")
                print("💡 Wait 1 minute or switch to a key from another Google Cloud project.\n")
            else:
                print(f"\n⚠️ Client Error: {e}\n")
        except KeyboardInterrupt:
            print("\n\nSession ended with Ctrl+C. Bye!")
            break
        except Exception as error:
            print(f"\n❌ Error: {error}\n")

if __name__ == "__main__":
    main()
