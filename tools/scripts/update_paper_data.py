#!/usr/bin/env python3
"""
工具 3: 将最新数据写入论文元数据文件
用法: python3 update_paper_data.py <paper_id> <updates.json>

updates.json:
[
  {"point_id": 1, "latest_value": "4200", "latest_year": 2026, "source": "英伟达财报", "source_url": "..."},
  ...
]

输出位置标注，不做过时判定
"""

import json, sys, os
from pathlib import Path
from datetime import datetime

PAPERS_DIR = "papers"


def update_paper(paper_id, updates):
    json_path = Path(PAPERS_DIR) / f"{paper_id}.json"
    if not json_path.exists():
        return {"error": f"论文数据文件不存在: {json_path}"}

    paper_data = json.loads(json_path.read_text())
    data_points = paper_data.get("data_points", [])

    applied = []
    for upd in updates:
        pid = upd.get("point_id", 0) - 1  # 1-based → 0-based
        if pid < 0 or pid >= len(data_points):
            continue

        point = data_points[pid]
        point["updated"] = True
        point["latest_value"] = upd.get("latest_value", "")
        point["latest_year"] = upd.get("latest_year", 0)
        point["latest_source"] = upd.get("source", "")
        point["latest_source_url"] = upd.get("source_url", "")

        applied.append({
            "point_id": pid + 1,
            "location": point.get("location", {}),
            "original": point.get("original", ""),
            "context": point.get("context", ""),
            "latest_value": upd.get("latest_value", ""),
            "latest_year": upd.get("latest_year", 0),
            "source": upd.get("source", "")
        })

    paper_data["data_points"] = data_points
    paper_data["last_data_update"] = datetime.now().isoformat()
    json_path.write_text(json.dumps(paper_data, indent=2, ensure_ascii=False))

    # 生成位置报告
    position_report = []
    for a in applied:
        loc = a["location"]
        position_report.append(
            f"第{loc['paragraph']}段 第{loc['line']}行 "
            f"(offset={loc['offset']}) "
            f"「{a['original']}」 → {a['latest_value']} "
            f"(来源: {a['source']}, {a['latest_year']})"
        )

    return {
        "paper_id": paper_id,
        "total_data_points": len(data_points),
        "updated_count": len(applied),
        "positions": position_report,
        "details": applied
    }


if __name__ == "__main__":
    paper_id = sys.argv[1]
    updates_input = sys.argv[2] if len(sys.argv) > 2 else "[]"
    updates = json.loads(updates_input)
    result = update_paper(paper_id, updates)
    print(json.dumps(result, indent=2, ensure_ascii=False))
