import asyncio
import json
import websockets

from google.oauth2 import service_account
from google.cloud import speech_v1p1beta1 as speech

HOST, PORT = "0.0.0.0", 8765
EXPECTED_PATH = "/audio"
SAMPLE_RATE_HZ = 16000
LANGUAGE_CODE = "en-US"



with open("../.env/geminiap-key.json", "r") as f:
    SERVICE_ACCOUNT_JSON = json.load(f)


def make_speech_client():
    creds_info = SERVICE_ACCOUNT_JSON
    creds = service_account.Credentials.from_service_account_info(creds_info)
    return speech.SpeechAsyncClient(credentials=creds)

async def stt_session(ws):
    client = make_speech_client()

    streaming_config = speech.StreamingRecognitionConfig(
        config=speech.RecognitionConfig(
            encoding=speech.RecognitionConfig.AudioEncoding.LINEAR16,
            sample_rate_hertz=SAMPLE_RATE_HZ,
            language_code=LANGUAGE_CODE,
            enable_automatic_punctuation=True,
            model="latest_long",
        ),
        interim_results=True,
        single_utterance=False,
    )

    q = asyncio.Queue()

    async def recv_audio():
        async for msg in ws:
            if isinstance(msg, (bytes, bytearray)):
                b = memoryview(msg)
                if len(b) >= 2:
                    total = 0
                    count = 0
                    for i in range(0, len(b), 2):
                        s = int.from_bytes(b[i:i+2], "little", signed=True)
                        total += s*s
                        count += 1
                    rms = int((total / max(1, count)) ** 0.5)
                else:
                    rms = 0
                #print(f"recv {len(b)} bytes, rms={rms}")
                await q.put(bytes(b))
            else:
                await ws.send(f"[echo] {msg}")
        await q.put(None)

    async def req_gen():
        yield speech.StreamingRecognizeRequest(streaming_config=streaming_config)
        while True:
            chunk = await q.get()
            if chunk is None:
                break
            yield speech.StreamingRecognizeRequest(audio_content=chunk)

    recv_task = asyncio.create_task(recv_audio())
    try:
        responses = await client.streaming_recognize(requests=req_gen())
        async for r in responses:
            if not r.results:
                continue
            result = r.results[0]
            text = result.alternatives[0].transcript
            await ws.send(("[final] " if result.is_final else "[interim] ") + text)
    except Exception as e:
        try:
            await ws.send(f"[error] {e}")
        finally:
            pass
    await recv_task

def _ws_path(ws):
    return getattr(getattr(ws, "request", None), "path", None) or getattr(ws, "path", None) or "?"

async def handler(ws):
    path = _ws_path(ws)
    print(f"Client connected from {ws.remote_address}, path={path}")
    if path != EXPECTED_PATH:
        print(f"Warning: client path is {path}, expected {EXPECTED_PATH}")

    try:
        await stt_session(ws)
    finally:
        print("Client disconnected")

async def main():
    print(f"Listening on ws://{HOST}:{PORT}{EXPECTED_PATH}")
    async with websockets.serve(
        handler, HOST, PORT,
        max_size=None, max_queue=None,
        ping_interval=None, ping_timeout=None
    ):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
