# -*- coding: utf-8 -*-
"""
LLM bridge for the SmartParking AI customer-service agent.

Usage: python llm_bridge.py <request_json_path>

Reads a JSON request file:
  {base_url, api_key, model, messages, tools(optional), temperature, max_tokens}
Performs ONE OpenAI-compatible /v1/chat/completions call (with retries for the
gateway's transient "No available accounts" errors) and prints a JSON result:
  {"content": "...", "tool_calls": [{"id","type","function":{"name","arguments"}}], "finish_reason": "..."}
or {"error": "..."} on failure.

Only the Python standard library is used (urllib) so no extra pip installs are
required. All output is UTF-8.
"""
import sys
import os
import json
import time
import urllib.request
import urllib.error

if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
        sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

# Errors from the aggregator gateway that are worth retrying
_TRANSIENT = (
    'no available accounts', 'api_key_required', 'rate limit',
    'overloaded', 'timeout', 'connection reset', 'temporarily unavailable',
)


def extract_first_json(s):
    """Return the first balanced {...} object in s (handles the gateway's
    duplicated-arguments quirk e.g. '{}{}' -> '{}')."""
    if not s:
        return "{}"
    s = str(s).strip()
    start = s.find('{')
    if start < 0:
        return "{}"
    depth = 0
    in_str = False
    esc = False
    for i in range(start, len(s)):
        c = s[i]
        if in_str:
            if esc:
                esc = False
            elif c == '\\':
                esc = True
            elif c == '"':
                in_str = False
        else:
            if c == '"':
                in_str = True
            elif c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return s[start:i + 1]
    return s[start:]


def do_request(base_url, api_key, payload):
    url = base_url.rstrip('/') + '/v1/chat/completions'
    data = json.dumps(payload, ensure_ascii=False).encode('utf-8')
    req = urllib.request.Request(url, data=data, method='POST')
    req.add_header('Content-Type', 'application/json')
    if api_key:
        req.add_header('Authorization', 'Bearer ' + api_key)
    with urllib.request.urlopen(req, timeout=60) as resp:
        body = resp.read().decode('utf-8', errors='replace')
        return resp.status, body


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "no request file provided"}, ensure_ascii=False))
        sys.exit(1)
    try:
        with open(sys.argv[1], 'r', encoding='utf-8') as f:
            req = json.load(f)
    except Exception as e:
        print(json.dumps({"error": "bad request file: %s" % e}, ensure_ascii=False))
        sys.exit(1)

    base_url = req.get('base_url', '')
    api_key = req.get('api_key', '')
    model = req.get('model', 'glm-5.2')
    messages = req.get('messages', [])
    tools = req.get('tools')
    temperature = req.get('temperature', 0.7)
    max_tokens = req.get('max_tokens', 1024)

    if not base_url:
        print(json.dumps({"error": "missing base_url"}, ensure_ascii=False))
        sys.exit(0)

    payload = {
        "model": model,
        "messages": messages,
        "temperature": temperature,
        "max_tokens": max_tokens,
    }
    if tools:
        payload["tools"] = tools
        payload["tool_choice"] = "auto"

    last_err = ""
    for attempt in range(4):
        try:
            status, body = do_request(base_url, api_key, payload)
            try:
                data = json.loads(body)
            except Exception:
                last_err = "non-JSON response (HTTP %s): %s" % (status, body[:200])
                time.sleep(1 + attempt)
                continue

            if isinstance(data, dict) and data.get('error'):
                err = data['error']
                msg = err.get('message', str(err)) if isinstance(err, dict) else str(err)
                low = msg.lower()
                if any(t in low for t in _TRANSIENT):
                    last_err = msg
                    time.sleep(1.5 * (attempt + 1))
                    continue
                print(json.dumps({"error": msg}, ensure_ascii=False))
                sys.exit(0)

            choices = data.get('choices') or []
            if not choices:
                last_err = "no choices in response: %s" % body[:200]
                time.sleep(1 + attempt)
                continue

            msg0 = choices[0].get('message', {}) or {}
            content = msg0.get('content') or ""
            finish = choices[0].get('finish_reason', '')
            tool_calls = []
            for tc in (msg0.get('tool_calls') or []):
                fn = tc.get('function', {}) or {}
                args = fn.get('arguments', '')
                if isinstance(args, dict):
                    args = json.dumps(args, ensure_ascii=False)
                args = extract_first_json(args)
                tool_calls.append({
                    "id": tc.get('id', ''),
                    "type": tc.get('type', 'function'),
                    "function": {"name": fn.get('name', ''), "arguments": args},
                })
            print(json.dumps({
                "content": content,
                "tool_calls": tool_calls,
                "finish_reason": finish,
            }, ensure_ascii=False))
            sys.exit(0)

        except urllib.error.HTTPError as e:
            try:
                err_body = e.read().decode('utf-8', errors='replace')
            except Exception:
                err_body = ''
            last_err = "HTTP %s: %s" % (e.code, err_body[:200])
            time.sleep(2 + attempt * 2)
            continue
        except urllib.error.URLError as e:
            last_err = "network error: %s" % getattr(e, 'reason', e)
            time.sleep(2 + attempt * 2)
            continue
        except Exception as e:
            last_err = "exception: %s" % e
            time.sleep(1 + attempt)
            continue

    print(json.dumps({"error": last_err or "LLM 请求失败"}, ensure_ascii=False))
    sys.exit(0)


if __name__ == '__main__':
    main()
