#!/usr/bin/env python3
"""
工具 1: 从论文 TXT 提取所有时序数据点，返回位置信息
用法: python3 extract_data_points.py <paper.txt>
"""

import re, json, sys


def extract_data_points(text):
    patterns = [
        # "2023 年 xxx 达到/约为 3500 万片"
        (r'(\d{4})\s*年.*?'
         r'(达到|为|约|约为|达到约|超过|增至|增长至|下降至|降至|'
         r'提升至|减少至|增至约|降为|增长为|减少为|占|占比|超过|'
         r'突破|逼近|逼近|跌至|跌至约|降至约)\s*'
         r'([\d,.]+(?:\.\d+)?)\s*'
         r'(万|亿|千万|百万|千|百|%|美元|元|万片|亿条|万个|亿次|'
         r'GB|TB|PB|GWh|MWh|TWh|万亿美元|亿美元|万亿元|亿元|万亿|'
         r'亿人|万人|万人次|亿人次|亿辆|万辆|万台|亿台)', 'metric'),

        # "xxx 占比 35.2% 或 从 35.2% 增长到 47.1%"
        (r'(占比|达到|为|约|增至|增长至|提高到|提升到|提高到|'
         r'下降至|降至|降到|跌至)\s*'
         r'([\d,.]+(?:\.\d+)?)\s*'
         r'(%|倍)', 'percent'),

        # "规模/产值/市值 达/为 500 亿美元"
        (r'(规模|产值|市值|收入|营收|支出|投资|融资|估值|'
         r'总资产|净资产|销售额|GMV|MAU|DAU|用户数|下载量|'
         r'出货量|装机量|保有量|产销量)'
         r'(达|为|约|约为|达到|达到约|突破|超过)\s*'
         r'([\d,.]+(?:\.\d+)?)\s*'
         r'(万|亿|千万|百万|千|百|美元|元|万片|亿条|万个|亿次|'
         r'万亿美元|亿美元|万亿元|亿元|万亿|亿人|万人|万台|亿台)', 'scale'),

        # 纯数字+单位（年份可能在附近）
        (r'([\d,.]+(?:\.\d+)?)\s*'
         r'(万|亿|千万|百万|千|百|%|美元|元|万片|万台|万人|亿人|'
         r'GB|TB|PB|GWh|TWh|MWh|亿条|万个|亿次|万辆|亿辆|'
         r'亿台|万亿美元|亿美元|万亿元|亿元)\b', 'bare'),
    ]

    data_points = []
    seen_spans = set()

    for pattern, ptype in patterns:
        for match in re.finditer(pattern, text):
            start, end = match.span()
            if (start, end) in seen_spans:
                continue
            seen_spans.add((start, end))

            # 上下文（前后各 80 字符）
            ctx_start = max(0, start - 80)
            ctx_end = min(len(text), end + 80)
            context = text[ctx_start:ctx_end].replace('\n', ' ').strip()

            # 行号定位
            line_num = text[:start].count('\n') + 1

            # 段号定位（双换行分隔）
            para_num = text[:start].count('\n\n') + 1

            # 尝试读年份
            year_match = re.search(r'(20(?:0\d|1\d|2\d)\s*年)', context)
            year = int(year_match.group(1)[:4]) if year_match else 0

            data_points.append({
                "id": len(data_points) + 1,
                "original": match.group(0).strip(),
                "context": context,
                "location": {
                    "paragraph": para_num,
                    "line": line_num,
                    "offset": start,
                    "length": end - start
                },
                "inferred_year": year,
                "type": ptype
            })

    return data_points


if __name__ == "__main__":
    txt_path = sys.argv[1]
    text = open(txt_path).read()
    points = extract_data_points(text)
    print(json.dumps(points, indent=2, ensure_ascii=False))
