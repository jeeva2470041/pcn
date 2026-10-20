from google import genai

client = genai.Client()
MODEL = "gemini-3.5-flash-lite"

try:
    response = client.models.generate_content(
        model=MODEL,
        contents="Hello Gemini! Reply with a short greeting."
    )
    print("\nGemini response:")
    print(response.text)
except Exception as error:
    print("\nAPI request failed:")
    print(error)
