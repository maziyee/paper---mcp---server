#!/usr/bin/env python3
"""
工具 2: 对单个数据点搜索全网最新值
用法: python3 search_latest_data.py <metric_json>

输入 JSON: {"metric":"GPU出货量","value":"3500","unit":"万片","year":2023}
输出 JSON: {"latest_value":"4200","latest_year":2026,"source":"英伟达财报",...}
"""

import json, re, sys, urllib.request, urllib.parse


def search_arxiv(query, max_results=5):
    """arXiv API 搜索"""
    url = ("http://export.arxiv.org/api/query?"
           f"search_query=all:{urllib.parse.quote(query)}"
           f"&start=0&max_results={max_results}")
    try:
        resp = urllib.request.urlopen(url, timeout=10)
        return resp.read().decode()
    except Exception:
        return ""


def search_semantic_scholar(query, limit=5):
    """Semantic Scholar API 搜索"""
    url = ("https://api.semanticscholar.org/graph/v1/paper/search?"
           f"query={urllib.parse.quote(query)}&limit={limit}"
           "&fields=title,year,abstract")
    try:
        req = urllib.request.Request(url)
        resp = urllib.request.urlopen(req, timeout=10)
        data = json.loads(resp.read())
        return data.get("data", [])
    except Exception:
        return []


def search_github_trending(query):
    """GitHub 趋势搜索"""
    url = ("https://api.github.com/search/repositories?"
           f"q={urllib.parse.quote(query)}&sort=stars&order=desc&per_page=3")
    try:
        req = urllib.request.Request(url)
        req.add_header("Accept", "application/vnd.github.v3+json")
        req.add_header("User-Agent", "mcp-mt/1.0")
        resp = urllib.request.urlopen(req, timeout=10)
        data = json.loads(resp.read())
        return [{"name": r["full_name"], "stars": r["stargazers_count"]}
                for r in data.get("items", [])]
    except Exception:
        return []


def extract_number(text, unit, year):
    """从文本中提取最相关的数字"""
    # 1. 匹配年份附近有指定单位的数字
    patterns = [
        rf'(\d{{4}}).*?([\d,.]+)\s*{re.escape(unit)}',
        rf'([\d,.]+)\s*{re.escape(unit)}.*?(\d{{4}})',
        rf'([\d,.]+\.?\d*)\s*{re.escape(unit)}',
    ]
    results = []
    for pat in patterns:
        for m in re.finditer(pat, text):
            nums = [g for g in m.groups() if re.match(r'[\d,.]+', g)]
            if nums:
                yr = max(
                    [int(g) for g in m.groups() if g.isdigit() and len(g) == 4],
                    default=0)
                results.append({
                    "value": nums[0].replace(",", ""),
                    "year": yr
                })
    # 取最新年份的值
    results.sort(key=lambda x: x["year"], reverse=True)
    return results[0] if results else None


def search_latest(metric_info):
    """主搜索函数：从多个来源查找最新数据"""
    metric = metric_info.get("metric", "")
    if not metric and "original" in metric_info:
        metric = metric_info["original"]

    unit = metric_info.get("unit", "")
    old_year = metric_info.get("year", 2020)
    ctx = metric_info.get("context", "")

    # 构造搜索词（倾向于最新）
    queries = [
        f"{metric} 最新数据 2025 2026",
        f"{metric} {unit} 2026",
        f"{metric} 最新统计 年度报告",
        f"{metric} latest statistics 2025 2026",
    ]

    all_texts = []

    # arXiv 搜索
    for q in queries[:2]:
        xml_text = search_arxiv(q)
        if xml_text:
            all_texts.append(xml_text)

    # Semantic Scholar
    for q in queries[:2]:
        papers = search_semantic_scholar(q, limit=3)
        for p in papers:
            ab = p.get("abstract", "")
            if ab:
                all_texts.append(ab)
            tt = p.get("title", "")
            if tt:
                all_texts.append(tt)

    combined = "\n".join(all_texts)
    if not combined:
        return {"error": "no_search_results", "message": "未找到任何搜索结果"}

    # 提取数字
    extracted = extract_number(combined, unit, old_year)
    if not extracted:
        return {"error": "no_number_found", "message": f"未能从搜索结果中提取到 {unit} 相关数字"}

    # 找来源
    source_paper = None
    for p in search_semantic_scholar(metric, limit=1):
        source_paper = {
            "title": p.get("title"),
            "year": p.get("year"),
            "url": f"https://api.semanticscholar.org/paper/{p.get('paperId')}"
        }

    return {
        "latest_value": extracted["value"],
        "latest_unit": unit,
        "latest_year": extracted.get("year", 2025),
        "search_query": queries[0],
        "source": source_paper["title"] if source_paper else "Semantic Scholar / arXiv",
        "source_url": source_paper["url"] if source_paper else ""
    }


if __name__ == "__main__":
    inp = json.loads(sys.argv[1]) if len(sys.argv) > 1 else {}
    result = search_latest(inp)
    print(json.dumps(result, indent=2, ensure_ascii=False))
