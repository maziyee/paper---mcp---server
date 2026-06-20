#!/usr/bin/env python3
"""
Vision API Client — 调用 OpenAI 兼容的多模态视觉大模型。

用法:
  python3 vision_client.py <task_type> '<json_payload>'

task_type: analyze_image | ocr_image | compare_images | analyze_video

配置优先级: 环境变量 > config/server_config.json > 默认值
  VISION_BASE_URL   — API 地址, 如 http://localhost:8000/v1
  VISION_MODEL      — 模型名, 如 gpt-4o / Qwen2.5-VL-72B
  VISION_API_KEY    — API Key (可选, 本地模型可不设)

纯 Python 标准库, 零外部依赖。
"""

import json
import sys
import os
import base64
import mimetypes
import urllib.request
import urllib.error
from pathlib import Path


# ============================================================
# 配置加载
# ============================================================

def load_config():
    """
    加载配置: 环境变量 > config/server_config.json > 默认值
    返回 {"base_url": "...", "model": "...", "api_key": "..."}
    """
    cfg = {
        "base_url": "http://localhost:8000/v1",
        "model": "gpt-4o",
        "api_key": "not-needed",
    }

    # 1. 尝试读取 config/server_config.json
    script_dir = Path(__file__).resolve().parent
    config_path = script_dir.parent.parent / "config" / "server_config.json"
    try:
        with open(config_path, "r", encoding="utf-8") as f:
            j = json.load(f)
            vision_cfg = j.get("vision", {})
            if isinstance(vision_cfg, dict):
                for k in ("base_url", "model", "api_key"):
                    if k in vision_cfg and vision_cfg[k]:
                        cfg[k] = vision_cfg[k]
    except (FileNotFoundError, json.JSONDecodeError, KeyError):
        pass

    # 2. 环境变量覆盖 (仅 base_url 和 model，API key 只用配置文件)
    env_map = {
        "VISION_BASE_URL": "base_url",
        "VISION_MODEL": "model",
    }
    for env_name, cfg_key in env_map.items():
        val = os.environ.get(env_name, "").strip()
        if val:
            cfg[cfg_key] = val

    # 3. 规范化 base_url: 去掉末尾的 /chat/completions 后缀
    base = cfg["base_url"].rstrip("/")
    if base.endswith("/chat/completions"):
        base = base[: -len("/chat/completions")]
    cfg["base_url"] = base

    return cfg


# ============================================================
# 图片/视频处理
# ============================================================

MIME_MAP = {
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".png": "image/png",
    ".gif": "image/gif",
    ".webp": "image/webp",
    ".bmp": "image/bmp",
    ".svg": "image/svg+xml",
    ".tiff": "image/tiff",
    ".tif": "image/tiff",
    ".mp4": "video/mp4",
    ".avi": "video/x-msvideo",
    ".mov": "video/quicktime",
    ".mkv": "video/x-matroska",
    ".webm": "video/webm",
}


def get_mime_type(file_path: str) -> str:
    ext = Path(file_path).suffix.lower()
    if ext in MIME_MAP:
        return MIME_MAP[ext]
    mime, _ = mimetypes.guess_type(file_path)
    return mime or "application/octet-stream"


def is_url(source: str) -> bool:
    return source.startswith("http://") or source.startswith("https://")


def encode_local_file(file_path: str) -> str:
    """读取本地文件 → base64 data URI"""
    path = Path(file_path)
    if not path.exists():
        raise FileNotFoundError(f"文件不存在: {file_path}")
    if not path.is_file():
        raise ValueError(f"不是文件: {file_path}")
    mime = get_mime_type(file_path)
    data = path.read_bytes()
    b64 = base64.b64encode(data).decode("ascii")
    return f"data:{mime};base64,{b64}"


def fetch_remote_file(url: str) -> str:
    """下载远程文件 → base64 data URI"""
    req = urllib.request.Request(url, headers={"User-Agent": "paper-mcp/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            body = resp.read()
            content_type = resp.headers.get("Content-Type", "")
            mime = content_type.split(";")[0].strip() or "application/octet-stream"
            b64 = base64.b64encode(body).decode("ascii")
            return f"data:{mime};base64,{b64}"
    except urllib.error.HTTPError as e:
        raise RuntimeError(f"下载失败 HTTP {e.code}: {url}")
    except urllib.error.URLError as e:
        raise RuntimeError(f"连接失败: {e.reason}")


def get_data_uri(source: str) -> str:
    """智能分发: URL 远程下载, 本地路径 base64"""
    source = source.strip()
    if is_url(source):
        return fetch_remote_file(source)
    return encode_local_file(source)


def is_image(source: str) -> bool:
    mime = get_mime_type(source)
    return mime.startswith("image/") or (is_url(source) and not source.lower().endswith((".mp4", ".avi", ".mov", ".mkv", ".webm")))


def is_video(source: str) -> bool:
    mime = get_mime_type(source)
    return mime.startswith("video/")


# ============================================================
# Vision API 调用
# ============================================================

def call_vision_api(config: dict, messages: list, max_tokens: int = 2000,
                    temperature: float = 0.1) -> dict:
    """
    调用 OpenAI 兼容 chat completions API。
    返回 {"content": "...", "model": "...", "usage": {...}}
    """
    url = f"{config['base_url']}/chat/completions"
    headers = {
        "Content-Type": "application/json",
        "User-Agent": "paper-mcp/1.0",
    }
    if config.get("api_key") and config["api_key"] != "not-needed":
        headers["Authorization"] = f"Bearer {config['api_key']}"

    body = {
        "model": config["model"],
        "messages": messages,
        "max_tokens": max_tokens,
        "temperature": temperature,
    }

    data = json.dumps(body).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers=headers, method="POST")

    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            result = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        err_body = ""
        try:
            err_body = e.read().decode("utf-8")[:500]
        except Exception:
            pass
        raise RuntimeError(f"Vision API 返回 {e.code}: {err_body}")
    except urllib.error.URLError as e:
        raise RuntimeError(f"连接 Vision API 失败: {e.reason}")

    choices = result.get("choices", [])
    if not choices:
        raise RuntimeError("Vision API 返回了空的 choices")

    return {
        "content": choices[0].get("message", {}).get("content", ""),
        "model": result.get("model", config["model"]),
        "usage": result.get("usage", {}),
    }


# ============================================================
# 各 task_type 实现
# ============================================================

DEFAULT_ANALYZE_PROMPT = (
    "请详细描述这张图片的内容。如果包含图表，请说明："
    "图表类型、标题、横纵坐标含义、数据趋势、关键标注、异常点。"
    "如果是表格，请以结构化的方式描述表格内容和列含义。"
    "如果是实验图（如显微图像、电镜图等），请描述图像中的关键结构和特征。"
)


def analyze_image(config: dict, payload: dict) -> dict:
    """单张图片分析"""
    source = payload["image_source"]
    prompt = payload.get("prompt", "") or DEFAULT_ANALYZE_PROMPT
    max_tokens = int(payload.get("max_tokens", 2000))
    detail = payload.get("detail", "auto")

    data_uri = get_data_uri(source)

    messages = [{
        "role": "user",
        "content": [
            {"type": "text", "text": prompt},
            {"type": "image_url", "image_url": {"url": data_uri, "detail": detail}},
        ],
    }]

    result = call_vision_api(config, messages, max_tokens=max_tokens)
    result["task_type"] = "analyze_image"
    result["source"] = source
    return result


OCR_FORMAT_PROMPTS = {
    "plain": (
        "请提取这张图片中的所有文字内容。对于表格，以对齐的纯文本格式输出。"
        "只输出文字内容，不要添加额外说明。"
    ),
    "markdown": (
        "请提取这张图片中的所有文字内容，并以 Markdown 格式输出。"
        "保留标题层级、列表、表格结构等。表格请用 Markdown 表格格式。"
        "只输出文字内容，不要添加额外说明。"
    ),
    "json": (
        "请提取这张图片中的所有文字内容。"
        "返回一个 JSON 对象，格式为: "
        '{"blocks": [{"text": "...", "type": "heading|paragraph|list_item|table|caption|other"}]}'
        "不要添加额外说明，只输出 JSON。"
    ),
}


def ocr_image(config: dict, payload: dict) -> dict:
    """图片 OCR 文字提取"""
    source = payload["image_source"]
    format_type = payload.get("format", "plain")
    language = payload.get("language", "")
    max_tokens = int(payload.get("max_tokens", 4000))

    data_uri = get_data_uri(source)

    prompt = OCR_FORMAT_PROMPTS.get(format_type, OCR_FORMAT_PROMPTS["plain"])
    if language:
        prompt += f"\n目标语言: {language}。请确保正确识别这些语言的字符。"

    messages = [{
        "role": "user",
        "content": [
            {"type": "text", "text": prompt},
            {"type": "image_url", "image_url": {"url": data_uri, "detail": "high"}},
        ],
    }]

    result = call_vision_api(config, messages, max_tokens=max_tokens)
    result["task_type"] = "ocr_image"
    result["source"] = source
    result["format"] = format_type
    return result


DEFAULT_COMPARE_PROMPT = (
    "请对比这些图片，详细描述它们之间的差异和相似之处。"
    "如果包含图表，请对比数据趋势、数值范围、标注差异等。"
    "如果是实验图像，请对比结构、特征、质量差异等。"
)


def compare_images(config: dict, payload: dict) -> dict:
    """多图片对比 (2-4 张)"""
    sources = payload["image_sources"]
    prompt = payload.get("prompt", "") or DEFAULT_COMPARE_PROMPT
    max_tokens = int(payload.get("max_tokens", 4000))

    if not isinstance(sources, list):
        # 支持逗号分隔的字符串
        sources = [s.strip() for s in str(sources).split(",") if s.strip()]
    if len(sources) < 2:
        raise ValueError("至少需要 2 张图片进行对比")
    if len(sources) > 4:
        raise ValueError(f"最多支持 4 张图片, 收到了 {len(sources)} 张")

    content = [{"type": "text", "text": prompt}]
    for src in sources:
        data_uri = get_data_uri(src)
        content.append({
            "type": "image_url",
            "image_url": {"url": data_uri, "detail": "auto"},
        })

    messages = [{"role": "user", "content": content}]

    result = call_vision_api(config, messages, max_tokens=max_tokens)
    result["task_type"] = "compare_images"
    result["sources"] = sources
    return result


VIDEO_GUIDANCE = (
    "当前 Vision API 标准接口不直接支持视频分析。建议方案:\n"
    "1. 使用 ffmpeg 提取关键帧:\n"
    "   ffmpeg -i <video> -vf fps=1/5 keyframes/frame_%04d.png\n"
    "2. 对提取的帧图片使用 analyze_image 或 compare_images 工具进行分析\n"
    "3. 如果需要时序分析，可以每秒提取一帧并按顺序分析\n\n"
    "视频文件路径: {source}\n"
    "如果需要立即开始，请提供提取好的帧图片路径。"
)


def analyze_video(config: dict, payload: dict) -> dict:
    """视频分析 — 提供引导说明"""
    source = payload["video_source"]

    return {
        "task_type": "analyze_video",
        "content": VIDEO_GUIDANCE.format(source=source),
        "source": source,
        "model": config["model"],
        "usage": {},
    }


# ============================================================
# 入口
# ============================================================

TASK_HANDLERS = {
    "analyze_image": analyze_image,
    "ocr_image": ocr_image,
    "compare_images": compare_images,
    "analyze_video": analyze_video,
}


def main():
    # 强制 UTF-8 输出, 避免 Windows GBK 编码导致 JSON 解析失败
    sys.stdout.reconfigure(encoding='utf-8')
    if len(sys.argv) < 3:
        print(json.dumps({
            "error": f"用法: {sys.argv[0]} <task_type> '<json_payload>'",
            "task_types": list(TASK_HANDLERS.keys()),
        }, ensure_ascii=True))
        sys.exit(1)

    task_type = sys.argv[1]

    # 支持两种传参方式: 直接 JSON 字符串 或 --file <路径>
    if len(sys.argv) >= 4 and sys.argv[2] == "--file":
        with open(sys.argv[3], "r", encoding="utf-8") as f:
            payload_str = f.read()
    elif len(sys.argv) >= 3:
        payload_str = sys.argv[2]
    else:
        print(json.dumps({
            "error": f"用法: {sys.argv[0]} <task_type> '<json_payload>' 或 --file <路径>",
            "task_types": list(TASK_HANDLERS.keys()),
        }, ensure_ascii=True))
        sys.exit(1)

    if task_type not in TASK_HANDLERS:
        print(json.dumps({
            "error": f"未知 task_type: {task_type}",
            "available": list(TASK_HANDLERS.keys()),
        }, ensure_ascii=True))
        sys.exit(1)

    try:
        payload = json.loads(payload_str)
    except json.JSONDecodeError as e:
        print(json.dumps({"error": f"JSON 解析失败: {e}"}, ensure_ascii=True))
        sys.exit(1)

    try:
        config = load_config()
        handler = TASK_HANDLERS[task_type]
        result = handler(config, payload)
        print(json.dumps(result, ensure_ascii=True))
    except Exception as e:
        print(json.dumps({"error": str(e), "task_type": task_type}, ensure_ascii=True))
        sys.exit(1)


if __name__ == "__main__":
    main()
