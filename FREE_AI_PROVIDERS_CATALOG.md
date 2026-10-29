# Complete Free & Opt-In AI Providers Directory (98 Total: 68 Free + 30 Paid Opt-In)

All **68 Free Providers** (`5` No-Key + `52` Free-Key + `11` Local) and **30 Opt-In Paid Providers** (disabled by default) are also wired directly into **Falcon AI v2.0** (`Settings -> Providers & Hardware`, C++20 `provider_catalog.hpp`, `/api/providers`, and `releases/Falcon-AI-2.0.0-x64.exe`).

---

## Summary Count

| Group | Baseline Count | + 2026 Bonus Discovered | Total in Falcon AI | Default State |
| :--- | :---: | :---: | :---: | :--- |
| **1. Work with NO key & NO account** | **4** | **+1** (`Puter.js`) | **5** | **ON out-of-the-box** (~26–30+ models ready on fresh install) |
| **2. Free tier, needs a free key (sign up)** | **47** | **+5** (`GitHub Models`, `Chutes.ai`, `Vercel AI Gateway`, `ArliAI`, `GLHF.chat`) | **52** | Add free key in `Settings -> Providers` to add models to the Swarm |
| **3. Local: free, runs on your own computer** | **11** | — | **11** | **Free & unlimited** on your own RAM / CPU / GPU |
| **Total Free Providers** | **62** | **+6** | **68** | **$0 cost** |
| **4. Paid (off by default, opt-in per provider)** | **30** | — | **30** | **OFF by default** (explicit opt-in only) |

---

## 1. Work with No Key and No Account (4 Core + 1 Bonus = 5)

These work the moment you install with zero signup and zero API keys:

| # | Provider | Free Limit & Notes | OpenAI-Compatible Endpoint | Site |
| :-: | :--- | :--- | :--- | :--- |
| 1 | **OVHcloud AI Endpoints** | No key needed: ~2 requests/min per model per IP (17–20+ open-weight EU models) | `https://oai.endpoints.kepler.ai.cloud.ovh.net/v1` | `endpoints.ai.cloud.ovh.net` |
| 2 | **Kilo Gateway** | No key needed: ~200 requests/hour per IP on `:free` routes (`kilo-auto/free`) | `https://api.kilo.ai/api/gateway/v1` | `kilo.ai` |
| 3 | **LLM7** | No key needed (1 req at a time, 30–60 RPM); free token raises to 120 RPM (includes free `gpt-4o-mini`) | `https://api.llm7.io/v1` | `token.llm7.io` |
| 4 | **Pollinations** | No key needed; anonymous tier is rate-limited per IP | `https://text.pollinations.ai/openai` | `auth.pollinations.ai` |
| 5 *(Bonus)* | **Puter.js Serverless AI** | No API key needed; serverless SDK/gateway to 400+ models under fair-use | `https://api.puter.com/drivers/call` | `puter.com` |

---

## 2. Free Tier with a Free Key (47 Core + 5 Bonus = 52)

Add any key in **Settings → Providers** (`[0-gpu c++20]` button in top bar). Each key adds its free models to the 6-Agent Swarm:

| # | Provider | Free Limit | Get a Free Key |
| :-: | :--- | :--- | :--- |
| 1 | **OpenCode free** | Daily free cap (resets 00:00 UTC), free for a limited time | `opencode.ai` |
| 2 | **Groq** | 30 requests/min, 1,000–14,400 requests/day (free LPU tier, no card) | `console.groq.com` |
| 3 | **Google Gemini** | 15 requests/min, 20+ requests/day (free AI Studio tier; higher on Gemma 3) | `aistudio.google.com` |
| 4 | **Cerebras** | Free tier with daily token caps; sign-up may ask for verification/card | `cloud.cerebras.ai` |
| 5 | **NVIDIA** | ~40 requests/min, trial/developer evaluation use; some queues stall | `build.nvidia.com` |
| 6 | **Atria (Atria Dawn)** | Free preview (Google sign-in, free token grant; quotas tightened over time) | `api.atria-asi.ai` |
| 7 | **OpenRouter** | 20 requests/min, 50 requests/day on `:free` models (1,000/day after one-time $10 top-up) | `openrouter.ai` |
| 8 | **Mistral** | Free "Experiment" plan with rate limits | `console.mistral.ai` |
| 9 | **Cohere** | Trial key: ~1,000 calls/month | `dashboard.cohere.com` |
| 10 | **Cloudflare Workers AI** | 10,000 Neurons/day free | `dash.cloudflare.com` |
| 11 | **Z.ai (Zhipu)** | Free GLM Flash models (~1,000 req/day) | `z.ai` |
| 12 | **Ollama Cloud** | Free cloud tier with hourly/daily usage caps | `ollama.com` |
| 13 | **Hugging Face Router** | ~$0.10/month included free Serverless Router credit | `huggingface.co` |
| 14 | **ElectronHub** | Shared weekly free credit pool | `playground.electronhub.ai` |
| 15 | **xKiro** | Free plan: 5M tokens/day on free models | `xkiro.com` |
| 16 | **ACLIDE** | Free tier (limits set by the provider) | `aclide.com` |
| 17 | **Api.Airforce** | 1 request/min, 1,000 requests/day (free tier) | `api.airforce` |
| 18 | **DreamPrompting** | 100 requests/min, 5,000 requests/day (free tier) | `dreamprompting.com` |
| 19 | **Waterfall** | Free tier (limits set by the provider) | `getwaterfall.org` |
| 20 | **NaraRouter** | Free plan (needs Telegram verification) | `bynara.id` |
| 21 | **Routeway** | Free pool ~5–20 requests/min, 200/day | `routeway.ai` |
| 22 | **Requesty** | 200 requests/day shared across free models | `app.requesty.ai` |
| 23 | **Agnes AI** | Promotional $0 models | `agnes-ai.com` |
| 24 | **SEA-LION (AI Singapore)** | 10 requests/min free | `playground.sea-lion.ai` |
| 25 | **Router9** | Free tier (limits set by the provider) | `router9.com` |
| 26 | **Septor Labs** | Free tier (limits set by the provider) | `septorlabs.com` |
| 27 | **Aion Labs** | 15 requests/min, ~20K tokens/day (free tier) | `aionlabs.ai` |
| 28 | **CLōD** | Free tier (limits set by the provider) | `clod.io` |
| 29 | **Speka** | Free tier (limits set by the provider) | `speka.me` |
| 30 | **BazaarLink** | 10 requests/min, 50 requests/day (free tier) | `bazaarlink.ai` |
| 31 | **UnoRouter** | 1 request/min (free tier) | `unorouter.com` |
| 32 | **AMD Radeon Cloud** | Free tier (limits set by the provider) | `developer.amd.com.cn` |
| 33 | **Logfare** | Free tier (limits set by the provider) | `logfare.ai` |
| 34 | **ModelScope** | ~2,000 calls/day; needs Alibaba Cloud CN account with real-name verification | `modelscope.cn` |
| 35 | **AnyAPI** | 100K tokens/day on free models | `anyapi.ai` |
| 36 | **AINative Studio** | Advertised ~10M tokens/month free | `ainative.studio` |
| 37 | **OrcaRouter** | Free tier (limits set by the provider) | `orcarouter.ai` |
| 38 | **NavyAI** | 150K tokens/day, 20 requests/min | `api.navy` |
| 39 | **LongCat (Meituan)** | Daily free quota | `longcat.chat` |
| 40 | **Experiential Labs** | Shared monthly credit pool | `experientiallabs.ai` |
| 41 | **Kenari** | Free tier (limits set by the provider) | `kenari.id` |
| 42 | **SiliconFlow** | Permanent free small/coder models (e.g. Qwen2.5-7B, DeepSeek-R1-Distill-7B) | `cloud.siliconflow.com` |
| 43 | **Baidu Qianfan** | Free ERNIE Speed/Lite; Chinese real-name verification | `console.bce.baidu.com` |
| 44 | **Volcengine Ark** | Daily free quota per model; Chinese real-name verification | `console.volcengine.com` |
| 45 | **iFlytek Spark** | Spark Lite is permanently free | `console.xfyun.cn` |
| 46 | **B.AI** | Free tier (limits set by the provider) | `b.ai` |
| 47 | **Free.ai** | 30,000 tokens/day, 1,000 req/month, 60/min (~1,650 hidden prompt tokens/msg -> ~10–15 msgs/day; self-hosted models free) | `free.ai` |
| 48 *(Bonus)* | **GitHub Models** | 15 requests/min, 60–150 requests/day free with any GitHub PAT (GPT-4.1, GPT-4o, o4-mini, Llama, Phi) | `github.com/marketplace/models` |
| 49 *(Bonus)* | **Chutes.ai** | Free decentralized OSS community tier (DeepSeek, Qwen, Llama), no card | `chutes.ai` |
| 50 *(Bonus)* | **Vercel AI Gateway** | Zero-price automatic free route included with free account | `vercel.com/ai-gateway` |
| 51 *(Bonus)* | **ArliAI** | Free forever tier on 7B–12B Llama & Mistral open models (no card) | `arliai.com` |
| 52 *(Bonus)* | **GLHF.chat** | Free rate-limited open-weight model API for prototyping | `glhf.chat` |

---

## 3. Local: Free, Uses Your Own Computer (11)

Optional — runs models directly on your own RAM / CPU / GPU with zero cloud rate limits:

| # | Provider | Local Default Endpoint | Notes | Site |
| :-: | :--- | :--- | :--- | :--- |
| 1 | **Ollama (this computer)** | `http://127.0.0.1:11434/v1` | Free, runs on your own RAM/CPU/GPU | `ollama.com` |
| 2 | **LM Studio** | `http://127.0.0.1:1234/v1` | Free, runs on your own RAM/CPU/GPU | `lmstudio.ai` |
| 3 | **llama.cpp server / LocalAI / llamafile / MLX** | `http://127.0.0.1:8080/v1` | Free, runs on your own RAM/CPU/GPU | `github.com` |
| 4 | **vLLM / NVIDIA NIM / Lemonade** | `http://127.0.0.1:8001/v1` | Free, runs on your own RAM/CPU/GPU | `docs.vllm.ai` |
| 5 | **SGLang** | `http://127.0.0.1:30000/v1` | Free, runs on your own RAM/CPU/GPU | `docs.sglang.ai` |
| 6 | **Jan** | `http://127.0.0.1:1337/v1` | Free, runs on your own RAM/CPU/GPU | `jan.ai` |
| 7 | **GPT4All** | `http://127.0.0.1:4891/v1` | Free, runs on your own RAM/CPU/GPU | `www.nomic.ai` |
| 8 | **KoboldCpp** | `http://127.0.0.1:5001/v1` | Free, runs on your own RAM/CPU/GPU | `github.com` |
| 9 | **text-generation-webui / TabbyAPI** | `http://127.0.0.1:5000/v1` | Free, runs on your own RAM/CPU/GPU | `github.com` |
| 10 | **Local Model Runner (:12434 OpenAI API)** | `http://127.0.0.1:12434/engines/llama.cpp/v1` | Free localhost `:12434` GGUF runner endpoint *(Falcon AI uses zero container files natively)* | `docs.docker.com` |
| 11 | **Local (Built-in Native C++20 Engine)** | `builtin://cpp20-openmp` | **Unlimited**, 0 ms latency, runs directly on this machine even with 0 GPU | `built-in` |

---

## 4. Paid Providers You Can Opt Into (30 — Off by Default)

Disabled by default so Falcon AI never incurs charges unless you explicitly toggle a provider ON and paste an API key:

1. **OpenAI** (`platform.openai.com`)
2. **Anthropic Claude** (`console.anthropic.com`)
3. **Google Gemini (paid tier)** (`ai.google.dev`)
4. **xAI Grok** (`console.x.ai`)
5. **DeepSeek (official API)** (`platform.deepseek.com`)
6. **OpenRouter (all paid models)** (`openrouter.ai`)
7. **Perplexity** (`perplexity.ai`)
8. **Together AI** (`api.together.ai`)
9. **Fireworks AI** (`fireworks.ai`)
10. **DeepInfra** (`deepinfra.com`)
11. **Novita AI** (`novita.ai`)
12. **Hyperbolic** (`app.hyperbolic.xyz`)
13. **Nebius Token Factory** (`studio.nebius.ai`)
14. **SambaNova Cloud** (`cloud.sambanova.ai`)
15. **Moonshot AI (Kimi)** (`platform.moonshot.ai`)
16. **Alibaba Qwen (Model Studio, international)** (`dashscope.console.aliyun.com`)
17. **MiniMax** (`platform.minimaxi.com`)
18. **StepFun** (`platform.stepfun.com`)
19. **Azure OpenAI / Azure AI Foundry** (`ai.azure.com`)
20. **Amazon Bedrock** (`aws.amazon.com/bedrock`)
21. **Meta Model API** (`llama.developer.meta.com`)
22. **AI21 Labs (Jamba)** (`studio.ai21.com`)
23. **Inception (Mercury)** (`inceptionlabs.ai`)
24. **Writer (Palmyra)** (`dev.writer.com`)
25. **Upstage (Solar)** (`console.upstage.ai`)
26. **Featherless AI** (`featherless.ai`)
27. **Venice AI** (`venice.ai`)
28. **Baseten Model APIs** (`baseten.co`)
29. **Scaleway Generative APIs** (`console.scaleway.com`)
30. **OpenCode (your key, needs credits)** (`opencode.ai`)
