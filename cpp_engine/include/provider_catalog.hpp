#pragma once
/**
 * Falcon AI v2.0 — Complete 98-Provider Free & Opt-In AI Registry (C++20)
 *
 * Groups:
 *   1. no_key   (5: 4 core + 1 bonus)  — Works out-of-the-box with NO key & NO account (~28+ free models on fresh install)
 *   2. free_key (52: 47 core + 5 bonus) — Free tier with a free signup API key (each key adds its free models to the 6-Agent Swarm)
 *   3. local    (11)                    — Free, unlimited, runs on your own RAM/CPU/GPU
 *   4. paid     (30)                    — OFF by default; opt-in per provider only when toggled on + key added
 */

#include "json.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace openmesh {

struct AIProviderSpec {
    const char* id;
    const char* name;
    const char* group;         // "no_key" | "free_key" | "local" | "paid"
    const char* limit_notes;
    const char* site;
    const char* base_url;      // OpenAI-compatible /v1 endpoint where applicable
    const char* default_model;
    int free_models_est;
    bool default_enabled;
    bool is_bonus;
};

inline const std::vector<AIProviderSpec>& get_all_provider_specs() {
    static const std::vector<AIProviderSpec> kSpecs = {
        // =====================================================================
        // 1. WORK WITH NO KEY AND NO ACCOUNT (4 Core + 1 Bonus = 5)
        // Enabled by default on fresh install (~28+ models immediately ready)
        // =====================================================================
        {"ovhcloud", "OVHcloud AI Endpoints", "no_key",
         "No key needed: ~2 requests/min per model per IP (17+ EU open-weight models)",
         "endpoints.ai.cloud.ovh.net", "https://oai.endpoints.kepler.ai.cloud.ovh.net/v1", "Meta-Llama-3_3-70B-Instruct", 12, true, false},
        {"kilo", "Kilo Gateway", "no_key",
         "No key needed: ~200 requests/hour per IP on :free routes",
         "kilo.ai", "https://api.kilo.ai/api/gateway/v1", "kilo-auto/free", 8, true, false},
        {"llm7", "LLM7", "no_key",
         "No key needed (1 req at a time, 30-60 RPM); free token at token.llm7.io raises to 120 RPM",
         "token.llm7.io", "https://api.llm7.io/v1", "gpt-4o-mini", 6, true, false},
        {"pollinations", "Pollinations", "no_key",
         "No key needed; anonymous tier is rate-limited per IP",
         "auth.pollinations.ai", "https://text.pollinations.ai/openai", "openai", 4, true, false},
        {"puter_free", "Puter.js Serverless AI (Bonus)", "no_key",
         "No API key needed: browser/serverless gateway to 400+ models under fair-use",
         "puter.com", "https://api.puter.com/drivers/call", "gpt-4o-mini", 6, false, true},

        // =====================================================================
        // 2. FREE TIER WITH A FREE KEY (47 Core + 5 Bonus = 52)
        // Add key in Settings -> Providers to add its free models to the Swarm
        // =====================================================================
        {"opencode_free", "OpenCode free", "free_key",
         "Daily free cap (resets 00:00 UTC), free for a limited time",
         "opencode.ai", "https://opencode.ai/zen/v1", "deepseek-r1-free", 4, false, false},
        {"groq", "Groq", "free_key",
         "30 requests/min, 1,000-14,400 requests/day (free LPU tier, no card)",
         "console.groq.com", "https://api.groq.com/openai/v1", "llama-3.3-70b-versatile", 9, false, false},
        {"gemini_free", "Google Gemini", "free_key",
         "15 requests/min, free AI Studio tier (Gemma 3 & Gemini Flash)",
         "aistudio.google.com", "https://generativelanguage.googleapis.com/v1beta/openai", "gemini-2.5-flash", 6, false, false},
        {"cerebras", "Cerebras", "free_key",
         "Free tier with fast wafer-scale daily token caps",
         "cloud.cerebras.ai", "https://api.cerebras.ai/v1", "llama-3.3-70b", 4, false, false},
        {"nvidia_nim", "NVIDIA", "free_key",
         "About 40 requests/min, free developer evaluation tier",
         "build.nvidia.com", "https://integrate.api.nvidia.com/v1", "meta/llama-3.3-70b-instruct", 12, false, false},
        {"atria", "Atria (Atria Dawn)", "free_key",
         "Free preview (Google sign-in, free token grant)",
         "api.atria-asi.ai", "https://api.atria-asi.ai/v1", "atria-dawn", 3, false, false},
        {"openrouter_free", "OpenRouter", "free_key",
         "20 requests/min, 50 requests/day on :free models (1,000/day after $10 top-up)",
         "openrouter.ai", "https://openrouter.ai/api/v1", "meta-llama/llama-3.3-70b-instruct:free", 24, false, false},
        {"mistral", "Mistral", "free_key",
         "Free 'Experiment' plan with rate limits across Codestral & Mistral",
         "console.mistral.ai", "https://api.mistral.ai/v1", "mistral-small-latest", 8, false, false},
        {"cohere", "Cohere", "free_key",
         "Free trial key: ~1,000 calls/month (Command R/A)",
         "dashboard.cohere.com", "https://api.cohere.ai/compatibility/v1", "command-r-plus-08-2024", 5, false, false},
        {"cloudflare_ai", "Cloudflare Workers AI", "free_key",
         "10,000 Neurons/day free across Llama, Qwen, GLM & DeepSeek",
         "dash.cloudflare.com", "https://api.cloudflare.com/client/v4/accounts/ACCOUNT_ID/ai/v1", "@cf/meta/llama-3.3-70b-instruct-fp8-fast", 15, false, false},
        {"zai_zhipu", "Z.ai (Zhipu)", "free_key",
         "Free GLM-4-Flash models (~1,000 req/day)",
         "z.ai", "https://api.z.ai/api/paas/v4", "glm-4-flash", 3, false, false},
        {"ollama_cloud", "Ollama Cloud", "free_key",
         "Free cloud tier with hourly/daily usage caps",
         "ollama.com", "https://ollama.com/v1", "llama3.3", 6, false, false},
        {"hf_router", "Hugging Face Router", "free_key",
         "Monthly included free Serverless Inference Router credits",
         "huggingface.co", "https://router.huggingface.co/v1", "Qwen/Qwen2.5-Coder-32B-Instruct", 18, false, false},
        {"electronhub", "ElectronHub", "free_key",
         "Shared weekly free credit pool",
         "playground.electronhub.ai", "https://api.electronhub.ai/v1", "gpt-4o-mini", 10, false, false},
        {"xkiro", "xKiro", "free_key",
         "Free plan: 5M tokens/day on free models",
         "xkiro.com", "https://api.xkiro.com/v1", "deepseek-v3", 5, false, false},
        {"aclide", "ACLIDE", "free_key",
         "Free tier (limits set by the provider)",
         "aclide.com", "https://api.aclide.com/v1", "auto-free", 4, false, false},
        {"api_airforce", "Api.Airforce", "free_key",
         "1 request/min, 1,000 requests/day (free tier)",
         "api.airforce", "https://api.airforce/v1", "llama-3.3-70b", 6, false, false},
        {"dreamprompting", "DreamPrompting", "free_key",
         "100 requests/min, 5,000 requests/day (free tier)",
         "dreamprompting.com", "https://api.dreamprompting.com/v1", "gpt-4o-mini", 6, false, false},
        {"waterfall", "Waterfall", "free_key",
         "Free tier (limits set by the provider)",
         "getwaterfall.org", "https://api.getwaterfall.org/v1", "auto", 3, false, false},
        {"nararouter", "NaraRouter", "free_key",
         "Free plan (needs Telegram verification)",
         "bynara.id", "https://api.bynara.id/v1", "auto-free", 4, false, false},
        {"routeway", "Routeway", "free_key",
         "Free pool ~5-20 requests/min, 200/day",
         "routeway.ai", "https://api.routeway.ai/v1", "llama-3.3-70b:free", 6, false, false},
        {"requesty", "Requesty", "free_key",
         "200 requests/day shared across zero-priced free models",
         "app.requesty.ai", "https://router.requesty.ai/v1", "free/auto", 5, false, false},
        {"agnes_ai", "Agnes AI", "free_key",
         "Promotional $0 models",
         "agnes-ai.com", "https://api.agnes-ai.com/v1", "agnes-free", 3, false, false},
        {"sea_lion", "SEA-LION (AI Singapore)", "free_key",
         "10 requests/min free tier",
         "playground.sea-lion.ai", "https://api.sea-lion.ai/v1", "aisingapore/Llama-SEA-LION-v3-70B-IT", 4, false, false},
        {"router9", "Router9", "free_key",
         "Free tier (limits set by the provider)",
         "router9.com", "https://api.router9.com/v1", "auto", 4, false, false},
        {"septor_labs", "Septor Labs", "free_key",
         "Free tier (limits set by the provider)",
         "septorlabs.com", "https://api.septorlabs.com/v1", "auto", 3, false, false},
        {"aion_labs", "Aion Labs", "free_key",
         "15 requests/min, ~20K tokens/day (free tier, no card)",
         "aionlabs.ai", "https://api.aionlabs.ai/v1", "aion-1.0-mini", 4, false, false},
        {"clod", "CLoD", "free_key",
         "Free tier (limits set by the provider)",
         "clod.io", "https://api.clod.io/v1", "auto", 3, false, false},
        {"speka", "Speka", "free_key",
         "Free tier (limits set by the provider)",
         "speka.me", "https://api.speka.me/v1", "auto", 3, false, false},
        {"bazaarlink", "BazaarLink", "free_key",
         "10 requests/min, 50 requests/day (free tier)",
         "bazaarlink.ai", "https://api.bazaarlink.ai/v1", "auto", 4, false, false},
        {"unorouter", "UnoRouter", "free_key",
         "1 request/min (free tier)",
         "unorouter.com", "https://api.unorouter.com/v1", "auto", 4, false, false},
        {"amd_radeon_cloud", "AMD Radeon Cloud", "free_key",
         "Free developer tier on Instinct/Radeon accelerators",
         "developer.amd.com.cn", "https://api.developer.amd.com.cn/v1", "qwen2.5-coder-32b", 4, false, false},
        {"logfare", "Logfare", "free_key",
         "Free tier (limits set by the provider)",
         "logfare.ai", "https://api.logfare.ai/v1", "auto", 3, false, false},
        {"modelscope", "ModelScope", "free_key",
         "2,000 calls/day free (Alibaba Cloud CN real-name verification)",
         "modelscope.cn", "https://api-inference.modelscope.cn/v1", "Qwen/Qwen2.5-Coder-32B-Instruct", 6, false, false},
        {"anyapi", "AnyAPI", "free_key",
         "100K tokens/day on free models",
         "anyapi.ai", "https://api.anyapi.ai/v1", "auto-free", 5, false, false},
        {"ainative", "AINative Studio", "free_key",
         "Advertised ~10M tokens/month free",
         "ainative.studio", "https://api.ainative.studio/v1", "llama-3.3-70b", 5, false, false},
        {"orcarouter", "OrcaRouter", "free_key",
         "Free tier (limits set by the provider)",
         "orcarouter.ai", "https://api.orcarouter.ai/v1", "auto", 4, false, false},
        {"navyai", "NavyAI", "free_key",
         "150K tokens/day, 20 requests/min",
         "api.navy", "https://api.navy/v1", "gpt-4o-mini", 6, false, false},
        {"longcat", "LongCat (Meituan)", "free_key",
         "Daily free quota on LongCat-Flash models",
         "longcat.chat", "https://api.longcat.chat/openai/v1", "LongCat-Flash-Chat", 3, false, false},
        {"experiential", "Experiential Labs", "free_key",
         "Shared monthly credit pool",
         "experientiallabs.ai", "https://api.experientiallabs.ai/v1", "auto", 3, false, false},
        {"kenari", "Kenari", "free_key",
         "Free tier (limits set by the provider)",
         "kenari.id", "https://api.kenari.id/v1", "auto", 3, false, false},
        {"siliconflow", "SiliconFlow", "free_key",
         "Permanent free small/coder models (Qwen2.5-7B, DeepSeek-R1-Distill-7B)",
         "cloud.siliconflow.com", "https://api.siliconflow.cn/v1", "Qwen/Qwen2.5-Coder-7B-Instruct", 6, false, false},
        {"baidu_qianfan", "Baidu Qianfan", "free_key",
         "Free ERNIE Speed/Lite (CN real-name verification)",
         "console.bce.baidu.com", "https://qianfan.baidubce.com/v2", "ernie-speed-128k", 3, false, false},
        {"volcengine_ark", "Volcengine Ark", "free_key",
         "Daily free quota per Doubao/DeepSeek model (CN verification)",
         "console.volcengine.com", "https://ark.cn-beijing.volces.com/api/v3", "doubao-1-5-lite-32k", 4, false, false},
        {"iflytek_spark", "iFlytek Spark", "free_key",
         "Spark Lite is permanently free",
         "console.xfyun.cn", "https://spark-api-open.xf-yun.com/v1", "lite", 2, false, false},
        {"b_ai", "B.AI", "free_key",
         "Free tier (limits set by the provider)",
         "b.ai", "https://api.b.ai/v1", "auto", 3, false, false},
        {"free_ai", "Free.ai", "free_key",
         "Free plan: 30,000 tokens/day, 1,000 req/month, 60/min (self-hosted models free)",
         "free.ai", "https://api.free.ai/v1", "default", 3, false, false},
        // --- 5 Bonus 2026 Free-Key Providers ---
        {"github_models", "GitHub Models (Bonus)", "free_key",
         "Free with any GitHub PAT: 15 RPM, 60-150 req/day on GPT-4.1, GPT-4o, o4-mini, Llama & Phi",
         "github.com/marketplace/models", "https://models.inference.ai.azure.com", "gpt-4o-mini", 13, false, true},
        {"chutes_ai", "Chutes.ai (Bonus)", "free_key",
         "Free decentralized OSS community tier (DeepSeek, Qwen, Llama), no credit card",
         "chutes.ai", "https://llm.chutes.ai/v1", "deepseek-ai/DeepSeek-V3", 6, false, true},
        {"vercel_ai_gw", "Vercel AI Gateway (Bonus)", "free_key",
         "Zero-price automatic free routes included with free account",
         "vercel.com/ai-gateway", "https://ai-gateway.vercel.sh/v1", "openai/gpt-4o-mini", 4, false, true},
        {"arliai", "ArliAI (Bonus)", "free_key",
         "Free forever tier on 7B-12B Llama & Mistral open models (no card)",
         "arliai.com", "https://api.arliai.com/v1", "Mistral-Nemo-12B-Instruct-2407", 4, false, true},
        {"glhf_chat", "GLHF.chat (Bonus)", "free_key",
         "Free rate-limited open-weight model API for prototyping",
         "glhf.chat", "https://glhf.chat/api/openai/v1", "hf:Qwen/Qwen2.5-Coder-32B-Instruct", 6, false, true},

        // =====================================================================
        // 3. LOCAL: FREE, RUNS ON YOUR OWN COMPUTER (11)
        // Uses your own RAM/CPU/GPU with zero cloud limits
        // =====================================================================
        {"local_ollama", "Ollama (this computer)", "local",
         "Runs on your own computer: free, uses your RAM/CPU/GPU",
         "ollama.com", "http://127.0.0.1:11434/v1", "qwen2.5-coder:7b", 0, true, false},
        {"local_lmstudio", "LM Studio", "local",
         "Runs on your own computer: free, uses your RAM/CPU/GPU",
         "lmstudio.ai", "http://127.0.0.1:1234/v1", "local-model", 0, true, false},
        {"local_llamacpp", "llama.cpp server / LocalAI / llamafile / MLX", "local",
         "Runs on your own computer: free, uses your RAM/CPU/GPU",
         "github.com", "http://127.0.0.1:8080/v1", "local-gguf", 0, true, false},
        {"local_vllm", "vLLM / NVIDIA NIM / Lemonade", "local",
         "Runs on your own computer: free, uses your RAM/CPU/GPU",
         "docs.vllm.ai", "http://127.0.0.1:8001/v1", "local-vllm", 0, true, false},
        {"local_sglang", "SGLang", "local",
         "Runs on your own computer: free, uses your RAM/CPU/GPU",
         "docs.sglang.ai", "http://127.0.0.1:30000/v1", "local-sglang", 0, true, false},
        {"local_jan", "Jan", "local",
         "Runs on your own computer: free, uses your RAM/CPU/GPU",
         "jan.ai", "http://127.0.0.1:1337/v1", "local-jan", 0, true, false},
        {"local_gpt4all", "GPT4All", "local",
         "Runs on your own computer: free, uses your RAM/CPU/GPU",
         "www.nomic.ai", "http://127.0.0.1:4891/v1", "local-gpt4all", 0, true, false},
        {"local_koboldcpp", "KoboldCpp", "local",
         "Runs on your own computer: free, uses your RAM/CPU/GPU",
         "github.com", "http://127.0.0.1:5001/v1", "koboldcpp", 0, true, false},
        {"local_tabby_webui", "text-generation-webui / TabbyAPI", "local",
         "Runs on your own computer: free, uses your RAM/CPU/GPU",
         "github.com", "http://127.0.0.1:5000/v1", "local-webui", 0, true, false},
        {"local_model_runner", "Model Runner Local (:12434 OpenAI API)", "local",
         "Runs on your own computer: free localhost :12434 GGUF endpoint",
         "localhost:12434", "http://127.0.0.1:12434/engines/llama.cpp/v1", "local-runner", 0, true, false},
        {"local_falcon_cpp20", "Local (Falcon Built-in C++20 Engine)", "local",
         "Unlimited, 0ms latency, runs natively inside Falcon AI on any CPU even with 0 GPU",
         "built-in", "builtin://cpp20-openmp", "falcon-cpp20-architect", 9, true, false},

        // =====================================================================
        // 4. PAID PROVIDERS YOU CAN OPT INTO (30 — OFF BY DEFAULT)
        // Never called unless you explicitly toggle on + provide your API key
        // =====================================================================
        {"paid_openai", "OpenAI", "paid", "Paid opt-in (off by default)", "platform.openai.com", "https://api.openai.com/v1", "gpt-4.1", 0, false, false},
        {"paid_anthropic", "Anthropic Claude", "paid", "Paid opt-in (off by default)", "console.anthropic.com", "https://api.anthropic.com/v1", "claude-sonnet-4-20250514", 0, false, false},
        {"paid_gemini", "Google Gemini (paid tier)", "paid", "Paid opt-in (off by default)", "ai.google.dev", "https://generativelanguage.googleapis.com/v1beta/openai", "gemini-2.5-pro", 0, false, false},
        {"paid_xai", "xAI Grok", "paid", "Paid opt-in (off by default)", "console.x.ai", "https://api.x.ai/v1", "grok-3", 0, false, false},
        {"paid_deepseek", "DeepSeek (official API)", "paid", "Paid opt-in (off by default)", "platform.deepseek.com", "https://api.deepseek.com/v1", "deepseek-chat", 0, false, false},
        {"paid_openrouter", "OpenRouter (all paid models)", "paid", "Paid opt-in (off by default)", "openrouter.ai", "https://openrouter.ai/api/v1", "anthropic/claude-3.7-sonnet", 0, false, false},
        {"paid_perplexity", "Perplexity", "paid", "Paid opt-in (off by default)", "perplexity.ai", "https://api.perplexity.ai", "sonar-pro", 0, false, false},
        {"paid_together", "Together AI", "paid", "Paid opt-in (off by default)", "api.together.ai", "https://api.together.xyz/v1", "meta-llama/Llama-3.3-70B-Instruct-Turbo", 0, false, false},
        {"paid_fireworks", "Fireworks AI", "paid", "Paid opt-in (off by default)", "fireworks.ai", "https://api.fireworks.ai/inference/v1", "accounts/fireworks/models/deepseek-v3", 0, false, false},
        {"paid_deepinfra", "DeepInfra", "paid", "Paid opt-in (off by default)", "deepinfra.com", "https://api.deepinfra.com/v1/openai", "Qwen/Qwen2.5-Coder-32B-Instruct", 0, false, false},
        {"paid_novita", "Novita AI", "paid", "Paid opt-in (off by default)", "novita.ai", "https://api.novita.ai/v3/openai", "deepseek/deepseek-v3", 0, false, false},
        {"paid_hyperbolic", "Hyperbolic", "paid", "Paid opt-in (off by default)", "app.hyperbolic.xyz", "https://api.hyperbolic.xyz/v1", "Qwen/Qwen2.5-Coder-32B-Instruct", 0, false, false},
        {"paid_nebius", "Nebius Token Factory", "paid", "Paid opt-in (off by default)", "studio.nebius.ai", "https://api.studio.nebius.ai/v1", "Qwen/Qwen2.5-Coder-32B-Instruct", 0, false, false},
        {"paid_sambanova", "SambaNova Cloud", "paid", "Paid opt-in (off by default)", "cloud.sambanova.ai", "https://api.sambanova.ai/v1", "Meta-Llama-3.3-70B-Instruct", 0, false, false},
        {"paid_moonshot", "Moonshot AI (Kimi)", "paid", "Paid opt-in (off by default)", "platform.moonshot.ai", "https://api.moonshot.ai/v1", "kimi-k2", 0, false, false},
        {"paid_qwen_intl", "Alibaba Qwen (Model Studio, international)", "paid", "Paid opt-in (off by default)", "dashscope.console.aliyun.com", "https://dashscope-intl.aliyuncs.com/compatible-mode/v1", "qwen-max", 0, false, false},
        {"paid_minimax", "MiniMax", "paid", "Paid opt-in (off by default)", "platform.minimaxi.com", "https://api.minimaxi.chat/v1", "MiniMax-Text-01", 0, false, false},
        {"paid_stepfun", "StepFun", "paid", "Paid opt-in (off by default)", "platform.stepfun.com", "https://api.stepfun.com/v1", "step-2-16k", 0, false, false},
        {"paid_azure", "Azure OpenAI / Azure AI Foundry", "paid", "Paid opt-in (off by default)", "ai.azure.com", "https://YOUR_RESOURCE.openai.azure.com/openai/deployments/MODEL", "gpt-4o", 0, false, false},
        {"paid_bedrock", "Amazon Bedrock", "paid", "Paid opt-in (off by default)", "aws.amazon.com/bedrock", "https://bedrock-runtime.us-east-1.amazonaws.com/openai/v1", "anthropic.claude-3-5-sonnet", 0, false, false},
        {"paid_meta", "Meta Model API", "paid", "Paid opt-in (off by default)", "llama.developer.meta.com", "https://api.llama.com/compat/v1", "Llama-4-Maverick-17B-128E-Instruct-FP8", 0, false, false},
        {"paid_ai21", "AI21 Labs (Jamba)", "paid", "Paid opt-in (off by default)", "studio.ai21.com", "https://api.ai21.com/studio/v1", "jamba-1.5-large", 0, false, false},
        {"paid_inception", "Inception (Mercury)", "paid", "Paid opt-in (off by default)", "inceptionlabs.ai", "https://api.inceptionlabs.ai/v1", "mercury-coder-small", 0, false, false},
        {"paid_writer", "Writer (Palmyra)", "paid", "Paid opt-in (off by default)", "dev.writer.com", "https://api.writer.com/v1", "palmyra-x-004", 0, false, false},
        {"paid_upstage", "Upstage (Solar)", "paid", "Paid opt-in (off by default)", "console.upstage.ai", "https://api.upstage.ai/v1/solar", "solar-pro", 0, false, false},
        {"paid_featherless", "Featherless AI", "paid", "Paid opt-in (off by default)", "featherless.ai", "https://api.featherless.ai/v1", "Qwen/Qwen2.5-72B-Instruct", 0, false, false},
        {"paid_venice", "Venice AI", "paid", "Paid opt-in (off by default)", "venice.ai", "https://api.venice.ai/api/v1", "llama-3.3-70b", 0, false, false},
        {"paid_baseten", "Baseten Model APIs", "paid", "Paid opt-in (off by default)", "baseten.co", "https://inference.baseten.co/v1", "deepseek-ai/DeepSeek-V3", 0, false, false},
        {"paid_scaleway", "Scaleway Generative APIs", "paid", "Paid opt-in (off by default)", "console.scaleway.com", "https://api.scaleway.ai/v1", "llama-3.3-70b-instruct", 0, false, false},
        {"paid_opencode", "OpenCode (your key, needs credits)", "paid", "Paid opt-in (off by default)", "opencode.ai", "https://opencode.ai/zen/v1", "claude-sonnet-4", 0, false, false}
    };
    return kSpecs;
}

struct UserProviderState {
    bool enabled = false;
    std::string api_key;
    std::string custom_base_url;
    std::string custom_model;
};

class ProviderPoolManager {
public:
    explicit ProviderPoolManager(const std::filesystem::path& save_path = "workspace_providers.json")
        : save_path_(save_path) {
        for (const auto& s : get_all_provider_specs()) {
            UserProviderState st;
            st.enabled = s.default_enabled;
            states_[s.id] = st;
        }
        load_from_disk();
    }

    JsonValue to_summary_json() const {
        std::lock_guard<std::mutex> lk(mtx_);
        JsonValue arr = JsonValue::array();
        int cnt_no_key = 0, cnt_free_key = 0, cnt_local = 0, cnt_paid = 0;
        int active_providers = 0, active_models = 0;

        for (const auto& s : get_all_provider_specs()) {
            std::string grp(s.group);
            if (grp == "no_key") ++cnt_no_key;
            else if (grp == "free_key") ++cnt_free_key;
            else if (grp == "local") ++cnt_local;
            else if (grp == "paid") ++cnt_paid;

            auto it = states_.find(s.id);
            UserProviderState st = (it != states_.end()) ? it->second : UserProviderState{s.default_enabled, "", "", ""};

            bool ready = false;
            if (grp == "no_key" || grp == "local") {
                ready = st.enabled;
            } else {
                ready = st.enabled && !st.api_key.empty();
            }
            if (ready) {
                ++active_providers;
                active_models += std::max(1, s.free_models_est);
            }

            JsonValue item = JsonValue::object();
            item["id"] = s.id;
            item["name"] = s.name;
            item["group"] = s.group;
            item["limit_notes"] = s.limit_notes;
            item["site"] = s.site;
            item["base_url"] = st.custom_base_url.empty() ? s.base_url : st.custom_base_url;
            item["model"] = st.custom_model.empty() ? s.default_model : st.custom_model;
            item["free_models_est"] = s.free_models_est;
            item["is_bonus"] = s.is_bonus;
            item["enabled"] = st.enabled;
            item["has_key"] = !st.api_key.empty();
            item["key_preview"] = st.api_key.empty()
                ? ""
                : (st.api_key.size() > 8 ? st.api_key.substr(0, 4) + "..." + st.api_key.substr(st.api_key.size() - 3) : "****");
            item["ready_in_team"] = ready;
            arr.push_back(item);
        }

        JsonValue out = JsonValue::object();
        out["counts"] = JsonValue::object({
            {"no_key", cnt_no_key},
            {"free_key", cnt_free_key},
            {"local", cnt_local},
            {"total_free", cnt_no_key + cnt_free_key + cnt_local},
            {"paid_opt_in", cnt_paid},
            {"active_in_swarm", active_providers},
            {"available_team_models", active_models}
        });
        out["providers"] = arr;
        return out;
    }

    void update_provider(const JsonValue& body) {
        std::lock_guard<std::mutex> lk(mtx_);
        std::string pid = body.get_str("id");
        if (pid.empty()) return;
        auto& st = states_[pid];
        if (body.has("api_key")) {
            std::string k = body.get_str("api_key");
            if (k == "__CLEAR__") {
                st.api_key.clear();
                st.enabled = false;
            } else if (!k.empty()) {
                st.api_key = k;
                st.enabled = true; // Adding a key automatically adds its free models to the team!
            }
        }
        if (body.has("enabled")) {
            st.enabled = body.get_bool("enabled", st.enabled);
        }
        if (body.has("base_url")) st.custom_base_url = body.get_str("base_url");
        if (body.has("model")) st.custom_model = body.get_str("model");
        save_to_disk_unlocked();
    }

    // Returns ordered list of active external OpenAI-compatible endpoints (base_url, api_key, model, name)
    struct EndpointCandidate {
        std::string id;
        std::string name;
        std::string base_url;
        std::string api_key;
        std::string model;
    };

    std::vector<EndpointCandidate> get_failover_candidates(bool include_no_key_cloud = false) const {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<EndpointCandidate> out;

        // Priority 1: User-configured Free-Key & Opted-In Paid keys (explicitly added by user)
        for (const auto& s : get_all_provider_specs()) {
            std::string grp(s.group);
            if (grp != "free_key" && grp != "paid") continue;
            auto it = states_.find(s.id);
            if (it == states_.end() || !it->second.enabled || it->second.api_key.empty()) continue;
            out.push_back({
                s.id, s.name,
                it->second.custom_base_url.empty() ? s.base_url : it->second.custom_base_url,
                it->second.api_key,
                it->second.custom_model.empty() ? s.default_model : it->second.custom_model
            });
        }

        // Priority 2: Instant-On No-Key Free Cloud Providers (when requested)
        if (include_no_key_cloud) {
            for (const auto& s : get_all_provider_specs()) {
                if (std::string(s.group) != "no_key") continue;
                auto it = states_.find(s.id);
                if (it == states_.end() || !it->second.enabled) continue;
                out.push_back({
                    s.id, s.name,
                    it->second.custom_base_url.empty() ? s.base_url : it->second.custom_base_url,
                    it->second.api_key.empty() ? "anonymous" : it->second.api_key,
                    it->second.custom_model.empty() ? s.default_model : it->second.custom_model
                });
            }
        }
        return out;
    }

private:
    void load_from_disk() {
        try {
            if (!std::filesystem::exists(save_path_)) return;
            std::ifstream f(save_path_);
            std::stringstream ss;
            ss << f.rdbuf();
            JsonValue j = JsonValue::parse(ss.str());
            if (!j.is_object()) return;
            for (const auto& s : get_all_provider_specs()) {
                if (!j.has(s.id)) continue;
                const auto& item = j[s.id];
                auto& st = states_[s.id];
                st.enabled = item.get_bool("enabled", st.enabled);
                st.api_key = item.get_str("api_key", "");
                st.custom_base_url = item.get_str("custom_base_url", "");
                st.custom_model = item.get_str("custom_model", "");
            }
        } catch (...) {}
    }

    void save_to_disk_unlocked() const {
        try {
            JsonValue j = JsonValue::object();
            for (const auto& kv : states_) {
                JsonValue item = JsonValue::object();
                item["enabled"] = kv.second.enabled;
                item["api_key"] = kv.second.api_key;
                item["custom_base_url"] = kv.second.custom_base_url;
                item["custom_model"] = kv.second.custom_model;
                j[kv.first] = item;
            }
            std::ofstream f(save_path_);
            f << j.dump(2);
        } catch (...) {}
    }

    mutable std::mutex mtx_;
    std::filesystem::path save_path_;
    std::map<std::string, UserProviderState> states_;
};

} // namespace openmesh
