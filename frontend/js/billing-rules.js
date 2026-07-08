
const user = checkAuth();
if (user) initSidebar();
let allLots = [];
let allRules = [];
let currentLotName = '';
let currentFilter = 'active';
async function loadLots() {
    const res = await get('/api/parking/list');
    if (!res || !res.ok || !res.data.lots) return;
    allLots = res.data.lots || [];
    const sel = document.getElementById('lot-selector');
    if (!sel || allLots.length === 0) return;
    sel.innerHTML = allLots.map(l =>
        `<option value="${escapeHtml(l.P_name)}">${escapeHtml(l.P_name)}</option>`    ).join('');
    sel.value = allLots[0].P_name;
    currentLotName = allLots[0].P_name;
    loadRules();
    loadPlans();
}
function onLotChange() {
    const sel = document.getElementById('lot-selector');
    currentLotName = sel.value;
    loadRules();
    loadPlans();
}
function filterRules(mode) {
    currentFilter = mode;
    document.getElementById('btn-filter-active').className = mode === 'active' ? 'btn btn-primary btn-sm' : 'btn btn-default btn-sm';
    document.getElementById('btn-filter-all').className = mode === 'all' ? 'btn btn-primary btn-sm' : 'btn btn-default btn-sm';
    renderRules();
}
async function loadRules() {
    const res = await get('/api/parking/billing-rules');
    if (!res || !res.ok || !res.data.rules) {
        document.getElementById('rules-container').innerHTML = '<p style="color:#999">加载失败</p>';
        return;
    }
    allRules = res.data.rules;
    renderRules();
}
function renderRules() {
    const container = document.getElementById('rules-container');
    let rules = allRules;
        if (currentLotName) {
        rules = rules.filter(r => r.P_name === currentLotName || !r.P_name || r.P_name === '');
    }
        if (currentFilter === 'active') {
        rules = rules.filter(r => r.is_active);
    }
    if (rules.length === 0) {
        container.innerHTML = '<div class="card"><p style="color:#999;text-align:center;padding:20px">该停车场暂无计费规则</p></div>';
        return;
    }
    const typeLabels = {
        standard: { name: '标准计费', cls: 'type-standard' },
        tiered: { name: '阶梯计费', cls: 'type-tiered' },
        member: { name: '会员计费', cls: 'type-member' },
        special: { name: '特殊车辆', cls: 'type-special' }
    };
    container.innerHTML = rules.map(r => {
        const tl = typeLabels[r.rule_type] || { name: r.rule_type, cls: 'type-standard' };
        const isActive = r.is_active;
        const activeBadge = isActive ? '' : '<span style="margin-left:8px;font-size:11px;color:#999;background:#f5f5f5;padding:1px 6px;border-radius:3px">未启用</span>';
        let tierHtml = '';
        if (r.rule_type === 'tiered' && r.tier_config) {
            try {
                const tiers = JSON.parse(r.tier_config);
                if (tiers && tiers.length > 0) {
                    tierHtml = `<table class="tier-table"><thead><tr><th>时段</th><th>费率</th></tr></thead><tbody>` +
                        tiers.map(t => `<tr><td>前 ${t.hours} 小时</td><td>¥${parseFloat(t.rate).toFixed(2)}/时</td></tr>`).join('') +
                        `</tbody></table>`;
                }
            } catch (e) {  }
        }
        return `<div class="rule-card" style="opacity:${isActive ? '1' : '0.6'}">
            <div style="display:flex;justify-content:space-between;align-items:center">
                <div>
                    <strong style="font-size:16px">${escapeHtml(r.rule_name)}</strong>
                    <span class="rule-type-tag ${tl.cls}" style="margin-left:8px">${tl.name}</span>
                    ${activeBadge}
                </div>
                ${r.P_name ? `<span style="font-size:12px;color:#999">${escapeHtml(r.P_name)}</span>` : ''}
            </div>
            <div class="rule-detail">
                <div class="rule-detail-item">
                    <div class="value">${r.free_minutes}<span style="font-size:13px;font-weight:400">分钟</span></div>
                    <div class="label">免费时长</div>
                </div>
                <div class="rule-detail-item">
                    <div class="value">¥${parseFloat(r.hourly_rate).toFixed(2)}</div>
                    <div class="label">每小时费率</div>
                </div>
                <div class="rule-detail-item">
                    <div class="value">¥${parseFloat(r.max_daily_fee).toFixed(2)}</div>
                    <div class="label">日封顶费用</div>
                </div>
            </div>
            ${r.description ? `<p style="font-size:13px;color:#666;margin-top:10px;padding-top:10px;border-top:1px solid #f0f0f0">${escapeHtml(r.description)}</p>` : ''}
            ${tierHtml}
        </div>`;
    }).join('');
}
async function loadPlans() {
    const container = document.getElementById('plans-container');
    let url = '/api/pass-plans';
    if (currentLotName) url += '?P_name=' + encodeURIComponent(currentLotName);
    const res = await get(url);
    if (!res || !res.ok || !res.data.plans || res.data.plans.length === 0) {
                if (currentLotName) {
            const fallback = await get('/api/pass-plans');
            if (fallback && fallback.ok && fallback.data.plans && fallback.data.plans.length > 0) {
                renderPlans(container, fallback.data.plans);
                return;
            }
        }
        container.innerHTML = '<p style="color:#999">暂无可用套餐</p>';
        return;
    }
    renderPlans(container, res.data.plans);
}
function renderPlans(container, plans) {
    container.innerHTML = plans.map(p => `
        <div style="display:flex;align-items:center;gap:16px;padding:12px 0;border-bottom:1px solid #f0f0f0">
            <div style="min-width:100px">
                <strong style="font-size:15px">${escapeHtml(p.plan_name)}</strong>
                <span style="color:#999;font-size:12px;margin-left:6px">${p.duration_days}天</span>
            </div>
            <div style="color:#ff4d4f;font-size:18px;font-weight:bold">¥${parseFloat(p.price).toFixed(2)}</div>
            <div style="color:#666;font-size:13px">${p.description ? escapeHtml(p.description) : ''}</div>
            <div style="margin-left:auto;font-size:12px;color:#999">日均 ¥${(p.price / p.duration_days).toFixed(2)}</div>
        </div>
    `).join('');
}
loadLots();
