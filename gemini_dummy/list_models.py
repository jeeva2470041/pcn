from google import genai

client = genai.Client()
print("Models supporting text generation:\n")

for model in client.models.list():
    actions = getattr(model, "supported_actions", []) or []
    if "generateContent" in actions:
        print(model.name)
