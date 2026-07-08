# 安全与凭据约定

## 公开仓库中**绝不**提交的内容

| 数据 | 典型位置 |
|------|----------|
| MySQL 用户名 / 密码 | `config/db_config.json` |
| LLM API Key | `config/db_config.json` |
| 任何服务的访问令牌 | 任意代码、配置文件 |

`config/db_config.json` 已在 `.gitignore` 中，不会被 Git 追踪。
`config/db_config.example.json` 中的所有敏感字段均为占位符 (`your_password_here`、`your_llm_api_key_here`)，可以被公开提交。

## 默认凭据

首次初始化后，系统会创建默认管理员账号：

- 用户名：`admin`
- 密码：`admin123`

**生产环境部署后请立即修改默认密码**（登录后在「个人中心」或通过管理员重置）。

## 代码提交前检查清单

- [ ] 没有 `password = "xxxx"` / `api_key = "sk-xxx"` 这类字面量
- [ ] 没有把 `config/db_config.json` 加入提交
- [ ] 没有把 `.vs/`、`build/`、`*.exe` 这类构建/IDE 产物加入提交
