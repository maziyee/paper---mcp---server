#!/usr/bin/env python3
"""
CNKI Search — 从 cnki-skills 的 cnki-search + cnki-parse-results 提取
使用 Playwright 操控 Chromium 搜索知网并返回结构化结果。

用法: python3 cnki_search.py <关键词> [--max-results N]

输出 JSON:
{
  "query": "半导体",
  "total": "1,234",
  "page": "1/42",
  "results": [
    {
      "n": 1,
      "title": "...",
      "url": "...",
      "authors": "...",
      "journal": "...",
      "date": "...",
      "citations": "...",
      "downloads": "...",
      "database": "..."
    },
    ...
  ]
}
"""

import json
import sys
import argparse
from playwright.sync_api import sync_playwright, TimeoutError as PlaywrightTimeout


CNKI_SEARCH_URL = "https://kns.cnki.net/kns8s/search"


def search_cnki(query: str, max_results: int = 20) -> dict:
    """搜索知网并返回结构化结果"""
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context(
            viewport={"width": 1280, "height": 800},
            user_agent=(
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                "AppleWebKit/537.36 (KHTML, like Gecko) "
                "Chrome/120.0.0.0 Safari/537.36"
            ),
        )
        page = context.new_page()

        try:
            # Step 1: Navigate to CNKI search
            page.goto(CNKI_SEARCH_URL, wait_until="domcontentloaded", timeout=15000)

            # Step 2: Execute search + extract (from cnki-search SKILL.md)
            result = page.evaluate(
                """async (query) => {
                // Wait for search input
                await new Promise((r, j) => {
                    let n = 0;
                    const c = () => {
                        if (document.querySelector('input.search-input')) r();
                        else if (++n > 30) j('timeout: input not found');
                        else setTimeout(c, 500);
                    };
                    c();
                });

                // Check captcha
                const outer = document.querySelector('#tcaptcha_transform_dy');
                if (outer && outer.getBoundingClientRect().top >= 0)
                    return { error: 'captcha' };

                // Fill and submit
                const input = document.querySelector('input.search-input');
                input.value = query;
                input.dispatchEvent(new Event('input', { bubbles: true }));
                document.querySelector('input.search-btn')?.click();

                // Wait for results
                await new Promise((r, j) => {
                    let n = 0;
                    const c = () => {
                        if (document.body.innerText.includes('条结果')) r();
                        else if (++n > 30) j('timeout: results not loaded');
                        else setTimeout(c, 500);
                    };
                    c();
                });

                // Check captcha again
                const outer2 = document.querySelector('#tcaptcha_transform_dy');
                if (outer2 && outer2.getBoundingClientRect().top >= 0)
                    return { error: 'captcha' };

                // Extract results (from cnki-parse-results SKILL.md)
                const rows = document.querySelectorAll('.result-table-list tbody tr');
                const checkboxes = document.querySelectorAll('.result-table-list tbody input.cbItem');
                const results = Array.from(rows).map((row, i) => {
                    const titleLink = row.querySelector('td.name a.fz14');
                    const authors = Array.from(
                        row.querySelectorAll('td.author a.KnowledgeNetLink') || []
                    ).map(a => a.innerText?.trim());
                    const journal = row.querySelector('td.source a')?.innerText?.trim() || '';
                    const date = row.querySelector('td.date')?.innerText?.trim() || '';
                    const database = row.querySelector('td.data')?.innerText?.trim() || '';
                    const citations = row.querySelector('td.quote')?.innerText?.trim() || '';
                    const downloads = row.querySelector('td.download')?.innerText?.trim() || '';
                    const isOnlineFirst = !!row.querySelector('td.name .marktip');

                    return {
                        n: i + 1,
                        title: titleLink?.innerText?.trim() || '',
                        url: titleLink?.href || '',
                        exportId: checkboxes[i]?.value || '',
                        authors: authors.join('; '),
                        journal,
                        date,
                        database,
                        citations,
                        downloads,
                        isOnlineFirst
                    };
                });

                return {
                    query,
                    total: document.querySelector('.pagerTitleCell')
                           ?.innerText?.match(/([\\d,]+)/)?.[1] || '0',
                    page: document.querySelector('.countPageMark')?.innerText || '1/1',
                    results
                };
            }""",
                query,
            )

            if isinstance(result, dict) and result.get("error"):
                return {"error": result["error"],
                        "message": "知网要求验证码，请在浏览器中手动完成"}

            # Trim results to max
            if "results" in result:
                result["results"] = result["results"][:max_results]

            return result

        except PlaywrightTimeout as e:
            return {"error": "timeout", "message": str(e)}
        except Exception as e:
            return {"error": "exception", "message": str(e)}
        finally:
            browser.close()


def main():
    parser = argparse.ArgumentParser(description="CNKI Search Tool")
    parser.add_argument("query", help="Search keywords (Chinese or English)")
    parser.add_argument("--max-results", type=int, default=20,
                        help="Max results to return (default: 20)")
    args = parser.parse_args()

    result = search_cnki(args.query, args.max_results)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
