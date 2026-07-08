
const user = checkAuth();
if (user) initSidebar();
let pieChart = null;
function applyPermUI() {
    const canCheckIn = hasPerm('vehicle.checkin');
    const canCheckOut = hasPerm('vehicle.checkout');
    const cardVehicle = document.getElementById('card-vehicle-ops');
    if (cardVehicle) cardVehicle.style.display = (canCheckIn || canCheckOut) ? '' : 'none';
    if (!canCheckIn) {
        const btn = document.getElementById('btn-checkin');
        if (btn) btn.style.display = 'none';
    }
        const btnCO = document.getElementById('btn-checkout');
    if (btnCO) btnCO.style.display = 'none';
        const billingSel = document.getElementById('billing-type');
    const billingLabel = billingSel?.parentElement?.querySelector('label');
    if (user.role === 'user' && billingSel) {
        billingSel.style.display = 'none';
        if (billingLabel) billingLabel.style.display = 'none';
    }
    const btnPlate = document.getElementById('btn-plate-recognize');
    if (btnPlate) btnPlate.style.display = hasPerm('plate.recognize') ? '' : 'none';
    document.getElementById('card-pass-plans').style.display = hasPerm('balance.view') ? '' : 'none';
            const isRegularUser = (user.role === 'user');
    document.getElementById('card-balance').style.display = (hasPerm('balance.view') && isRegularUser) ? '' : 'none';
    document.getElementById('card-my-billing').style.display = (hasPerm('balance.view') && isRegularUser) ? '' : 'none';
    document.getElementById('card-recent-records').style.display = hasPerm('vehicle.query') ? '' : 'none';
    document.getElementById('card-prediction').style.display = hasPerm('report.view') ? '' : 'none';
    document.getElementById('card-interceptions').style.display = hasPerm('vehicle.blacklist') ? '' : 'none';
    document.getElementById('card-plate-recognition').style.display = hasPerm('plate.recognize') ? '' : 'none';
    document.getElementById('card-parked-vehicles').style.display = hasPerm('vehicle.query') ? '' : 'none';
    document.getElementById('card-settings').style.display = hasPerm('parking.settings') ? '' : 'none';
        if (user.role === 'user') {
        const btnReport = document.getElementById('btn-report');
        if (btnReport) btnReport.style.display = 'none';
        const goalBar = document.getElementById('goal-bar');
        if (goalBar) goalBar.style.display = 'none';
        const heatmap = document.getElementById('card-heatmap');
        if (heatmap) heatmap.style.display = 'none';
    }
}
function initPieChart() {
    const dom = document.getElementById('pie-chart');
    if (!dom) return;
    pieChart = echarts.init(dom);
    pieChart.setOption({
        tooltip: { trigger: 'item', formatter: '{b}: {c} ({d}%)' },
        legend: { bottom: 0, itemWidth: 12, itemHeight: 12, textStyle: { fontSize: 12 } },
        series: [{
            type: 'pie', radius: ['40%', '65%'], center: ['50%', '45%'],
            avoidLabelOverlap: false,
            itemStyle: { borderRadius: 6, borderColor: '#fff', borderWidth: 2 },
            label: { show: true, formatter: '{b}\n{c}', fontSize: 12 },
            data: [
                { value: 0, name: '已占用', itemStyle: { color: '#ff4d4f' } },
                { value: 0, name: '已预约', itemStyle: { color: '#faad14' } },
                { value: 0, name: '剩余车位', itemStyle: { color: '#52c41a' } }
            ]
        }]
    });
}
async function loadParkingLots() {
    const res = await get('/api/parking/list');
    const sel = document.getElementById('parking-selector');
    if (!res || !res.ok || !sel || !res.data.lots) return;
    const lots = res.data.lots;
    sel.innerHTML = lots.map(l => {
        const avail = l.P_total_count - l.P_current_count - l.P_reserve_count;
        return `<option value="${escapeHtml(l.P_name)}">${escapeHtml(l.P_name)} (${avail}/${l.P_total_count})</option>`;
    }).join('');
        if (lots.length > 0 && !sel.dataset.loaded) {
        sel.dataset.loaded = '1';
        currentLot = lots[0].P_name;
        sel.value = currentLot;
    }
        const checkinLot = document.getElementById('checkin-lot');
    if (checkinLot) {
        checkinLot.innerHTML = lots.map(l =>
            `<option value="${escapeHtml(l.P_name)}">${escapeHtml(l.P_name)}</option>`        ).join('');
    }
        loadStatus();
    loadPassPlans();
}
let currentLot = '';
function onParkingChange() {
    const sel = document.getElementById('parking-selector');
    if (sel) currentLot = sel.value;
    loadStatus();
    loadPassPlans();
}
async function loadStatus() {
    let url = '/api/parking/status';
    if (currentLot) url += '?P_name=' + encodeURIComponent(currentLot);
    const res = await get(url);
    if (!res || !res.ok) return;
    const d = res.data;
    document.getElementById('stat-total').textContent = d.P_total_count;
    document.getElementById('stat-occupied').textContent = d.P_current_count;
    document.getElementById('stat-reserved').textContent = d.P_reserve_count;
    document.getElementById('stat-available').textContent = d.P_available_count;
    if (pieChart) {
        pieChart.setOption({ series: [{ data: [
            { value: d.P_current_count, name: '已占用', itemStyle: { color: '#ff4d4f' } },
            { value: d.P_reserve_count, name: '已预约', itemStyle: { color: '#faad14' } },
            { value: d.P_available_count, name: '剩余车位', itemStyle: { color: '#52c41a' } }
        ]}]});
    }
        updateGoals(d);
}
function updateGoals(d) {
    const occBar = document.getElementById('goal-occ-bar');
    const dailyBar = document.getElementById('goal-daily-bar');
    const monthBar = document.getElementById('goal-month-bar');
    if (!occBar || !d || user.role === 'user') return;
        const total = d.P_total_count || 1;
    const occupied = d.P_current_count || 0;
    const occRate = Math.min(occupied / total * 100, 100);
    occBar.style.width = occRate + '%';
    occBar.style.background = occRate > 80 ? 'linear-gradient(90deg,#ff6b6b,#ee5a24)' : occRate > 50 ? 'linear-gradient(90deg,#f9ca24,#f0932b)' : 'linear-gradient(90deg,#2ed573,#00aa7f)';
    document.getElementById('goal-occ').textContent = Math.round(occRate) + '%';
        const targetDaily = total * d.P_fee * 8 * 0.6;
    get('/api/report/summary').then(r => {
        if (!r || !r.ok) return;
        const today = parseFloat(r.data.today_income || 0);
        const dailyPct = Math.min(today / targetDaily * 100, 100);
        dailyBar.style.width = dailyPct + '%';
        dailyBar.style.background = 'linear-gradient(90deg,#a29bfe,#6c5ce7)';
        document.getElementById('goal-daily').textContent = '¥' + today.toFixed(0) + '/' + Math.round(targetDaily);
        const month = parseFloat(r.data.month_income || 0);
        const targetMonth = targetDaily * 30;
        const monthPct = Math.min(month / targetMonth * 100, 100);
        monthBar.style.width = monthPct + '%';
        monthBar.style.background = monthPct > 80 ? 'linear-gradient(90deg,#ff6b6b,#ee5a24)' : 'linear-gradient(90deg,#2ed573,#00aa7f)';
        document.getElementById('goal-month').textContent = '¥' + Math.round(month) + '/' + Math.round(targetMonth);
    });
}
async function loadRecentRecords() {
    const tbody = document.getElementById('recent-records');
    if (!tbody || !hasPerm('vehicle.query')) return;
    const res = await get('/api/vehicle/query');
    if (!res || !res.ok || !res.data.records || res.data.records.length === 0) {
        const hint = user.role === 'user' ? '暂无与您相关的记录' : '暂无记录';
        tbody.innerHTML = `<tr><td colspan="5" style="text-align:center;color:#999">${hint}</td></tr>`;
        return;
    }
    if (!res || !res.ok || !res.data.records || res.data.records.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;color:#999">暂无记录</td></tr>';
        return;
    }
    tbody.innerHTML = res.data.records.slice(0, 10).map(r => `
        <tr>
            <td><strong>${escapeHtml(r.license_plate)}</strong></td>
            <td>${formatDateTime(r.check_in_time)}</td>
            <td>${formatDateTime(r.check_out_time)}</td>
            <td>${formatFee(r.fee)}</td>
            <td>${r.check_out_time ? '<span class="badge badge-success">已出库</span>' : '<span class="badge badge-primary">停放中</span>'}</td>
        </tr>`).join('');
}
let _parkedData = [];
async function loadParkedVehicles() {
    const tbody = document.getElementById('parked-vehicles-list');
    if (!tbody || !hasPerm('vehicle.query')) return;
    const plate = document.getElementById('parked-search-input')?.value.trim() || '';
    const res = await get('/api/vehicle/parked' + (plate ? '?plate=' + encodeURIComponent(plate) : ''));
    if (!res || !res.ok || !res.data.records || res.data.records.length === 0) {
        const hint = user.role === 'user' ? '暂无与您相关的在场车辆' : '暂无在场车辆';
        tbody.innerHTML = `<tr><td colspan="6" style="text-align:center;color:#999">${hint}</td></tr>`;
        _parkedData = [];
        return;
    }
    if (!res || !res.ok || !res.data.records || res.data.records.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" style="text-align:center;color:#999">暂无在场车辆</td></tr>';
        _parkedData = [];
        return;
    }
    _parkedData = res.data.records;
    const canCheckOut = hasPerm('vehicle.checkout');
    tbody.innerHTML = _parkedData.map(r => {
        const hoursParked = (Date.now() - new Date(r.check_in_time).getTime()) / 3600000;
        const isOvertime = hoursParked > 24;
        return `<tr>
            <td><strong>${escapeHtml(r.license_plate)}</strong></td>
            <td>${escapeHtml(r.P_name || r.location)}</td>
            <td>${r.spot_number ? r.spot_number+'号' : '-'}</td>
            <td>${formatDateTime(r.check_in_time)}</td>
            <td><span style="color:${isOvertime?'#ff4d4f':'#666'};font-weight:${isOvertime?'600':'400'}">${r.duration || (r.check_out_time ? computeDurationText(r.check_in_time, r.check_out_time) : '计算中...')}${isOvertime?' ⚠️超24h':''}</span></td>
            <td>
                ${canCheckOut
                    ? `<button class="btn btn-danger btn-xs" onclick="quickCheckOutParked('${r.license_plate}')">出场</button>`
                    : '<span style="color:#999">-</span>'}
            </td>
        </tr>`;
    }).join('');
}
function exportParked() {
    if (!_parkedData.length) { showError('parked-alert', '没有可导出的数据'); return; }
    let csv = '车牌号,停车场,车位号,入库时间,已停时长,状态\n';
    _parkedData.forEach(r => {
        const hoursParked = (Date.now() - new Date(r.check_in_time).getTime()) / 3600000;
        const status = hoursParked > 24 ? '超时' : '正常';
        csv += `${r.license_plate},${r.P_name||r.location},${r.spot_number||''},${r.check_in_time},${r.duration||''},${status}\n`;
    });
    const blob = new Blob(['﻿' + csv], { type: 'text/csv;charset=utf-8;' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = '在场车辆_' + new Date().toISOString().split('T')[0] + '.csv';
    a.click();
    URL.revokeObjectURL(a.href);
}
async function quickCheckOutParked(plate) {
    const res = await post('/api/vehicle/checkout', { license_plate: plate });
    if (res && res.ok) {
        showSuccess('parked-alert', '车辆 ' + plate + ' 出库成功！费用: ' + formatFee(res.data.fee) + '。请在10分钟内驶离');
        showReceipt(plate, res.data);
        loadParkedVehicles(); loadStatus(); loadRecentRecords(); loadBalance();
    } else showError('parked-alert', res?.data?.error || '出库失败');
}
async function loadBalance() {
    if (!hasPerm('balance.view')) return;
    const balCard = document.getElementById('card-balance');
    if (balCard && balCard.style.display === 'none') return; 
    const res = await get('/api/balance');
    const balEl = document.getElementById('user-balance');
    const txEl = document.getElementById('balance-transactions');
    if (res && res.ok) {
        if (balEl) balEl.textContent = '¥' + parseFloat(res.data.balance).toFixed(2);
        if (txEl && res.data.transactions) {
            txEl.innerHTML = res.data.transactions.slice(0, 5).map(t =>
                `<div style="padding:4px 0;border-bottom:1px solid #f0f0f0;font-size:12px">
                    <span style="color:${t.amount>0?'#52c41a':'#ff4d4f'}">${t.amount>0?'+':''}${parseFloat(t.amount).toFixed(2)}</span>
                    <span style="color:#999;margin-left:8px">${escapeHtml(t.description)}</span>
                    <span style="float:right;color:#bbb">${formatDateTime(t.created_at)}</span>
                </div>`            ).join('') || '<p style="color:#999;font-size:12px">暂无交易记录</p>';
        }
    }
}
async function loadPassPlans() {
    const container = document.getElementById('pass-plans-container');
    if (!container || !hasPerm('balance.view')) return;
    let url = '/api/pass-plans';
    if (currentLot) url += '?P_name=' + encodeURIComponent(currentLot);
    const res = await get(url);
    if (!res || !res.ok || !res.data.plans) {
                if (currentLot) {
            const fallback = await get('/api/pass-plans');
            if (fallback && fallback.ok && fallback.data.plans && fallback.data.plans.length > 0) {
                renderPassPlans(container, fallback.data.plans);
                return;
            }
        }
        container.innerHTML = '<p style="color:#999">暂无可用套餐</p>';
        return;
    }
    if (res.data.plans.length === 0) {
                if (currentLot) {
            const fallback = await get('/api/pass-plans');
            if (fallback && fallback.ok && fallback.data.plans && fallback.data.plans.length > 0) {
                renderPassPlans(container, fallback.data.plans);
                return;
            }
        }
        container.innerHTML = '<p style="color:#999">暂无可用套餐</p>';
        return;
    }
    renderPassPlans(container, res.data.plans);
}
function renderPassPlans(container, plans) {
    container.innerHTML = plans.map(p => `
        <div class="pass-plan-card" style="border:1px solid #e8e8e8;border-radius:8px;padding:14px;margin-bottom:10px;cursor:pointer;transition:all 0.2s"
             onmouseover="this.style.borderColor='#1890ff';this.style.boxShadow='0 2px 8px rgba(24,144,255,0.15)'"
             onmouseout="this.style.borderColor='#e8e8e8';this.style.boxShadow='none'"
             onclick="openPurchaseModal(${p.id},'${escapeHtml(p.plan_name)}',${p.duration_days},${p.price},'${escapeHtml(p.description||'')}')">
            <div style="display:flex;justify-content:space-between;align-items:center">
                <div>
                    <strong style="font-size:15px">${escapeHtml(p.plan_name)}</strong>
                    <span style="color:#999;font-size:12px;margin-left:8px">${p.duration_days}天</span>
                </div>
                <span style="color:#ff4d4f;font-size:18px;font-weight:bold">¥${parseFloat(p.price).toFixed(2)}</span>
            </div>
            ${p.description ? '<p style="color:#666;font-size:12px;margin-top:6px">'+escapeHtml(p.description)+'</p>' : ''}
        </div>
    `).join('');
}
function openPurchaseModal(planId, planName, days, price, desc) {
    document.getElementById('purchase-plan-name').textContent = planName;
    document.getElementById('purchase-plan-desc').textContent = desc || (days+'天 | ¥'+price.toFixed(2));
    document.getElementById('purchase-plan-price').textContent = '¥' + price.toFixed(2);
    document.getElementById('purchase-plan-id').value = planId;
    document.getElementById('purchase-plate').value = '';
    document.getElementById('purchase-alert').innerHTML = '';
    showModal('purchase-modal');
}
async function confirmPurchase() {
    const planId = document.getElementById('purchase-plan-id').value;
    const plate = document.getElementById('purchase-plate').value.trim();
    if (!plate) { showError('purchase-alert', '请输入车牌号'); return; }
        const valRes = await post('/api/plate/validate', { license_plate: plate });
    if (valRes && valRes.ok && !valRes.data.valid) {
        showError('purchase-alert', valRes.data.message || '车牌号格式不正确');
        return;
    }
    const res = await post('/api/pass-plans/' + planId + '/purchase', { license_plate: plate });
    if (res && res.ok) {
        hideModal('purchase-modal');
        showSuccess('vehicle-alert', '套餐购买成功！');
        loadBalance();
        loadMyBilling();
    } else {
        showError('purchase-alert', res?.data?.error || '购买失败');
    }
}
function daysLeft(endDateStr) {
    if (!endDateStr) return null;
    const end = new Date(endDateStr + 'T23:59:59');
    if (isNaN(end)) return null;
    return Math.max(0, Math.ceil((end - new Date()) / 86400000));
}
async function loadMyBilling() {
    const card = document.getElementById('card-my-billing');
    if (!card || card.style.display === 'none') return;
    const box = document.getElementById('my-billing-content');
    if (!box) return;
        let myPasses = [];
    const passRes = await get(`/api/parking/monthly-passes?user_id=${user.id}`);
    if (passRes && passRes.ok && Array.isArray(passRes.data.passes)) {
        myPasses = passRes.data.passes.filter(p => p.user_id === user.id && p.is_active);
    }
    if (myPasses.length > 0) {
        box.innerHTML = myPasses.map(renderMyPassItem).join('');
        return;
    }
        let rule = null;
    const ruleRes = await get('/api/parking/billing-rules');
    if (ruleRes && ruleRes.ok && Array.isArray(ruleRes.data.rules)) {
        rule = ruleRes.data.rules.find(r => r.is_active) || null;
    }
    box.innerHTML = renderMyBillingRule(rule);
}
function renderMyPassItem(p) {
    const dl = daysLeft(p.end_date);
    const expiring = (dl !== null && dl <= 3);
    const leftTxt = (dl === null) ? '有效' : (dl <= 0 ? '今日到期' : `剩余 ${dl} 天`);
    const lotTxt = p.P_name ? `<span style="color:#1890ff;font-size:12px;margin-left:6px">${escapeHtml(p.P_name)}</span>` : '';
    return `
        <div style="border:1px solid #d6f0ff;background:linear-gradient(135deg,#e6f7ff 0%,#f6ffed 100%);border-radius:10px;padding:12px 14px;margin-bottom:8px">
            <div style="display:flex;justify-content:space-between;align-items:center">
                <div><span style="font-size:16px">🎫</span>
                    <strong style="font-size:15px;color:#1890ff;margin-left:4px">${escapeHtml(p.pass_type || '套餐')}</strong>${lotTxt}</div>
                <span class="badge ${expiring ? 'badge-danger' : 'badge-success'}" style="font-size:11px">${leftTxt}</span>
            </div>
            <div style="font-size:12px;color:#666;margin-top:8px">车牌：<b style="letter-spacing:1px">${escapeHtml(p.license_plate)}</b></div>
            <div style="font-size:12px;color:#666">有效期：${formatDate(p.start_date)} ~ ${formatDate(p.end_date)}</div>
            <div style="border-top:1px dashed #b7d8ff;margin:8px 0;padding-top:8px;font-size:12px;color:#52c41a">
                💰 套餐有效期内停车免费${p.fee ? `（已预付 ¥${parseFloat(p.fee).toFixed(2)}）` : ''}
            </div>
        </div>`;
}
function renderMyBillingRule(rule) {
    if (!rule) {
        return `<div style="text-align:center;padding:14px;color:#999;font-size:13px">暂无有效套餐，也未配置计费规则</div>`;
    }
    const desc = rule.description || '';
    const freeMin = (rule.free_minutes != null) ? rule.free_minutes : 30;
    const rate = (rule.hourly_rate != null) ? parseFloat(rule.hourly_rate) : 5;
    const cap = (rule.max_daily_fee != null) ? parseFloat(rule.max_daily_fee) : 0;
    const chip = (big, small, color) =>
        `<div style="flex:1;text-align:center;background:#fafafa;border-radius:8px;padding:8px 4px"><div style="font-size:14px;font-weight:600;color:${color}">${big}</div><div style="font-size:11px;color:#999">${small}</div></div>`;
    const chips = [
        chip(freeMin + ' 分', '免费时长', '#1890ff'),
        chip('¥' + rate.toFixed(2), '每小时', '#fa8c16'),
        cap > 0 ? chip('¥' + cap.toFixed(2), '每日封顶', '#ff4d4f') : ''
    ].filter(Boolean).join('');
    return `
        <div style="border:1px solid #f0f0f0;border-radius:10px;padding:12px 14px">
            <div style="display:flex;align-items:center;gap:6px;flex-wrap:wrap">
                <span style="font-size:16px">🅿️</span>
                <strong style="font-size:15px">${escapeHtml(rule.rule_name || '计费规则')}</strong>
                <span style="font-size:11px;color:#fff;background:#1890ff;border-radius:10px;padding:1px 8px">按次计费</span>
            </div>
            ${desc ? `<p style="font-size:12px;color:#666;margin:8px 0">${escapeHtml(desc)}</p>` : ''}
            <div style="display:flex;gap:8px;margin:8px 0">${chips}</div>
            <div style="border-top:1px dashed #eee;margin-top:8px;padding-top:8px;font-size:12px;color:#1890ff">
                💡 购买月卡 / 季卡 / 年卡可享包期优惠，停车更划算
            </div>
        </div>`;
}
async function loadMyPlates() {
    const container = document.getElementById('my-plates-list');
    if (!container) return;
    const res = await get('/api/user/plates');
    if (!res || !res.ok || !res.data.plates || res.data.plates.length === 0) {
        container.innerHTML = '<p style="color:#999;font-size:13px">暂未绑定车牌，绑定后可快速查询和入库</p>';
        return;
    }
    container.innerHTML = res.data.plates.map(p => `
        <div style="display:flex;align-items:center;justify-content:space-between;padding:6px 0;border-bottom:1px solid #f0f0f0">
            <span style="font-weight:600;letter-spacing:2px">${escapeHtml(p.license_plate)}</span>
            <button class="btn btn-danger btn-xs" onclick="unbindPlate(${p.id})" style="font-size:11px;padding:1px 8px">解绑</button>
        </div>
    `).join('');
}
async function bindPlate() {
    const input = document.getElementById('bind-plate-input');
    const plate = input.value.trim().toUpperCase();
    if (!plate) { showError('vehicle-alert', '请输入车牌号'); return; }
    const res = await post('/api/user/plates', { license_plate: plate });
    if (res && res.ok) {
        input.value = '';
        showSuccess('vehicle-alert', '车牌 ' + plate + ' 绑定成功！');
        loadMyPlates();
    } else {
        showError('vehicle-alert', res?.data?.error || '绑定失败');
    }
}
async function unbindPlate(id) {
    if (!confirm('确定解绑该车牌？')) return;
    const res = await del('/api/user/plates/' + id);
    if (res && res.ok) {
        showSuccess('vehicle-alert', '解绑成功');
        loadMyPlates();
    } else {
        showError('vehicle-alert', res?.data?.error || '解绑失败');
    }
}
async function loadBulletin() {
    const container = document.getElementById('bulletin-content');
    if (!container) return;
    const res = await get('/api/bulletin');
    if (res && res.ok) {
        const bulletins = res.data.bulletins || [];
        if (bulletins.length === 0) {
            container.innerHTML = '<p style="color:#999">暂无公告</p>';
            return;
        }
                container.innerHTML = bulletins.map((b, i) => {
            const mdHtml = typeof marked !== 'undefined' ? marked.parse(b.content) : b.content.replace(/\n/g, '<br>');
            const pinBadge = b.is_pinned ? '<span style="color:#ff4d4f;font-size:11px;margin-right:4px">[置顶]</span>' : '';
            return '<div style="' + (i > 0 ? 'margin-top:12px;padding-top:12px;border-top:1px solid #f0f0f0' : '') + '">' +
                pinBadge + '<div style="font-size:13px;line-height:1.8">' + mdHtml + '</div>';
        }).join('') + '</div>';
    }
}
async function loadPrediction() {
    if (!hasPerm('report.view')) return;
    const res = await get('/api/report/prediction');
    if (res && res.ok) {
        document.getElementById('predicted-total').textContent = '¥' + parseFloat(res.data.predicted_monthly).toFixed(2);
        document.getElementById('predicted-daily-avg').textContent = '¥' + parseFloat(res.data.daily_average).toFixed(2);
        document.getElementById('predicted-days-remaining').textContent = res.data.days_remaining;
    }
}
async function loadInterceptionCount() {
    if (!hasPerm('vehicle.blacklist')) return;
    const res = await get('/api/blacklist/interceptions/count');
    if (res && res.ok) {
        document.getElementById('interception-count').textContent = res.data.count;
    }
}
async function showSmartReport() {
    showModal('report-modal');
    document.getElementById('report-date').textContent = '报告生成: ' + new Date().toLocaleString('zh-CN');
    document.getElementById('report-body').innerHTML = '<div style="color:rgba(255,255,255,0.6);padding:40px 0;">⏳ 分析中...</div>';
        let statusUrl = '/api/parking/status';
    if (currentLot) statusUrl += '?P_name=' + encodeURIComponent(currentLot);
    const [statusRes, summaryRes, parkedRes, hourlyRes] = await Promise.all([
        get(statusUrl),
        get('/api/report/summary'),
        get('/api/vehicle/parked'),
        get('/api/report/hourly')
    ]);
    const d = statusRes?.data;
    const total = d?.P_total_count || 0;
    const occupied = d?.P_current_count || 0;
    const available = d?.P_available_count || 0;
    const occRate = total > 0 ? (occupied / total * 100).toFixed(0) : '0';
    const todayIncome = parseFloat(summaryRes?.data?.today_income || 0).toFixed(2);
    const monthIncome = parseFloat(summaryRes?.data?.month_income || 0).toFixed(0);
    const parkedCount = parkedRes?.data?.records?.length || 0;
        let peakHour = '-', peakCount = 0;
    if (hourlyRes?.data?.hours && hourlyRes.data.counts) {
        for (let i = 0; i < hourlyRes.data.hours.length; i++) {
            if (hourlyRes.data.counts[i] > peakCount) {
                peakCount = hourlyRes.data.counts[i];
                peakHour = hourlyRes.data.hours[i] + '时-' + (parseInt(hourlyRes.data.hours[i]) + 1) + '时';
            }
        }
    }
    const avgParked = occupied > 0 ? '约' + Math.round(parkedCount / Math.max(occupied, 1)) : '-';
    const efficiency = available >= total * 0.3 ? '充裕' : available > 0 ? '紧张' : '已满';
    setTimeout(() => {
        document.getElementById('report-body').innerHTML = `
            <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px;text-align:center;">
                <div class="report-card-in" style="background:linear-gradient(135deg,#ff6b6b,#ee5a24);border-radius:12px;padding:16px;box-shadow:0 4px 20px rgba(238,90,36,0.3);">
                    <div style="font-size:11px;opacity:0.8;margin-bottom:4px;letter-spacing:1px;">当前占用率</div>
                    <div style="font-size:44px;font-weight:800;text-shadow:0 2px 10px rgba(0,0,0,0.2);">${occRate}%</div>
                    <div style="font-size:12px;opacity:0.8;">${occupied}/${total} 车位</div>
                </div>
                <div class="report-card-in" style="background:linear-gradient(135deg,#2ed573,#0abde3);border-radius:12px;padding:16px;box-shadow:0 4px 20px rgba(10,189,227,0.3);">
                    <div style="font-size:11px;opacity:0.8;margin-bottom:4px;letter-spacing:1px;">今日收入</div>
                    <div style="font-size:44px;font-weight:800;text-shadow:0 2px 10px rgba(0,0,0,0.2);">¥${todayIncome}</div>
                    <div style="font-size:12px;opacity:0.8;">本月 ¥${monthIncome}</div>
                </div>
                <div class="report-card-in" style="background:linear-gradient(135deg,#a29bfe,#6c5ce7);border-radius:12px;padding:16px;box-shadow:0 4px 20px rgba(108,92,231,0.3);">
                    <div style="font-size:11px;opacity:0.8;margin-bottom:4px;letter-spacing:1px;">高峰时段</div>
                    <div style="font-size:44px;font-weight:800;text-shadow:0 2px 10px rgba(0,0,0,0.2);">${peakHour.split('-')[0]}</div>
                    <div style="font-size:12px;opacity:0.8;">至 ${peakHour.split('-')[1]||'--'}</div>
                </div>
                <div class="report-card-in" style="background:linear-gradient(135deg,#f9ca24,#f0932b);border-radius:12px;padding:16px;box-shadow:0 4px 20px rgba(240,147,43,0.3);">
                    <div style="font-size:11px;opacity:0.8;margin-bottom:4px;letter-spacing:1px;">在场车辆</div>
                    <div style="font-size:44px;font-weight:800;text-shadow:0 2px 10px rgba(0,0,0,0.2);">${parkedCount}</div>
                    <div style="font-size:12px;opacity:0.8;">车位状态: ${efficiency}</div>
                </div>
            </div>
            <div style="margin-top:16px;background:rgba(255,255,255,0.1);backdrop-filter:blur(10px);border-radius:12px;padding:16px;font-size:13px;text-align:left;border:1px solid rgba(255,255,255,0.15);">
                <div style="margin-bottom:8px;font-weight:600;letter-spacing:1px;">📌 运营摘要</div>
                <div style="display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid rgba(255,255,255,0.1);">
                    <span>车流量高峰</span><span style="font-weight:500;">${peakHour}（峰值 ${peakCount} 辆）</span>
                </div>
                <div style="display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid rgba(255,255,255,0.1);">
                    <span>平均每车占用</span><span style="font-weight:500;">${avgParked} 车位</span>
                </div>
                <div style="display:flex;justify-content:space-between;padding:6px 0;">
                    <span>推荐建议</span><span style="font-weight:500;color:${occRate > 80 ? '#ff6b6b' : occRate > 50 ? '#f9ca24' : '#2ed573'};">${occRate > 80 ? '⚠️ 考虑增加扩容或引导错峰' : occRate > 50 ? '✅ 运营正常' : '📣 可加大推广吸引客流'}</span>
                </div>
            </div>
        `;
    }, 300);
}
async function checkHealth() {
    const el = document.getElementById('health-status');
    if (!el) return;
    try {
        const res = await get('/api/health');
        if (res && res.ok && res.data.status === 'ok') {
            el.innerHTML = '✅ 服务正常 · ' + (res.data.db_pool || '') + ' · v' + (res.data.version || '');
            el.style.color = '#52c41a';
        } else {
            el.innerHTML = '⚠️ 服务异常';
            el.style.color = '#faad14';
        }
    } catch (e) {
        el.innerHTML = '❌ 连接失败';
        el.style.color = '#ff4d4f';
    }
}
async function loadHeatmap() {
    const container = document.getElementById('heatmap-chart');
    if (!container) return;
    const res = await get('/api/report/hourly');
    if (!res || !res.ok || !res.data.hours || !res.data.counts) {
        container.innerHTML = '<p style="color:#999;text-align:center;">暂无数据</p>';
        return;
    }
    const hours = res.data.hours.map(String);
    const counts = res.data.counts;
    if (hours.length === 0) { container.innerHTML = '<p style="color:#999;text-align:center;">暂无数据</p>'; return; }
    if (typeof echarts === 'undefined') { container.innerHTML = '<p style="color:#999;text-align:center;">图表库加载中...</p>'; return; }
    const chart = echarts.init(container);
    const allCounts = Array.from({length: 24}, (_, i) => { const idx = hours.indexOf(String(i)); return idx >= 0 ? counts[idx] : 0; });
    const maxVal = Math.max(...allCounts, 1);
    chart.setOption({
        tooltip: { trigger: 'axis', formatter: p => p[0].name + '<br>入场: ' + p[0].value + ' 辆' },
        grid: { left: 0, right: 0, top: 4, bottom: 4 },
        xAxis: { type: 'category', data: Array.from({length:24},(_,i)=>String(i)), axisLabel: { show: false }, axisTick: { show: false }, axisLine: { show: false }, splitLine: { show: false } },
        yAxis: { type: 'value', show: false, min: 0, max: maxVal, splitLine: { show: false } },
        series: [{
            type: 'bar', data: allCounts, barWidth: '80%',
            itemStyle: {
                color: p => {
                    const ratio = p.value / maxVal;
                    if (ratio === 0) return '#f2f2f2';
                    const g = Math.round(170 - ratio * 90);
                    return `rgb(0,${g},127)`;
                },
                borderRadius: [2, 2, 0, 0]
            },
            animationDelay: idx => idx * 30
        }]
    });
}
async function handleCheckIn() {
    const plate = document.getElementById('plate-input').value.trim();
    const billingSel = document.getElementById('billing-type');
    const billingType = (billingSel && billingSel.style.display !== 'none') ? billingSel.value : 'standard';
    if (!plate) { showError('vehicle-alert', '请输入车牌号'); return; }
    const body = { license_plate: plate, billing_type: 'standard' };
    if (currentLot) body.P_name = currentLot;
    const res = await post('/api/vehicle/checkin', body);
    if (res && res.ok) {
        showSuccess('vehicle-alert', `车辆 ${plate} 入库成功！`);
        document.getElementById('plate-input').value = '';
        loadStatus(); loadRecentRecords(); loadParkedVehicles();
    } else showError('vehicle-alert', res?.data?.error || '入库失败');
}
function showReceipt(plate, data) {
    const inTime = data.record ? data.record.check_in_time : (data.check_in_time || '');
    const outTime = data.record ? data.record.check_out_time : (data.check_out_time || '');
    const duration = data.record ? data.record.duration : (data.duration || '');
    const fee = data.fee || 0;
    document.getElementById('receipt-plate').textContent = plate;
    document.getElementById('receipt-in').textContent = formatDateTime(inTime);
    document.getElementById('receipt-out').textContent = formatDateTime(outTime);
    document.getElementById('receipt-duration').textContent = duration || computeDurationText(inTime, outTime) || '计算中...';
    document.getElementById('receipt-billing').textContent = '标准计费';
    document.getElementById('receipt-fee').textContent = '¥' + parseFloat(fee).toFixed(2);
    showModal('receipt-modal');
}
async function handleCheckOut() {
    const plate = document.getElementById('plate-input').value.trim();
    if (!plate) { showError('vehicle-alert', '请输入车牌号'); return; }
    const res = await post('/api/vehicle/checkout', { license_plate: plate });
    if (res && res.ok) {
        showSuccess('vehicle-alert', `车辆 ${plate} 出库成功！费用: ${formatFee(res.data.fee)}。请在10分钟内驶离`);
        showReceipt(plate, res.data);
        document.getElementById('plate-input').value = '';
        loadStatus(); loadRecentRecords(); loadBalance(); loadParkedVehicles();
    } else showError('vehicle-alert', res?.data?.error || '出库失败');
}
async function handlePlateRecognize() {
    const res = await post('/api/plate/recognize', {});
    if (res && res.ok) showSuccess('vehicle-alert', res.data?.message || '识别完成');
    else showError('vehicle-alert', res?.data?.error || res?.error || '识别失败');
}
function openRechargeModal() {
    document.getElementById('recharge-amount').value = 100;
    document.getElementById('recharge-alert').innerHTML = '';
    showModal('recharge-modal');
}
function setRechargeAmount(amount) {
    document.getElementById('recharge-amount').value = amount;
    document.querySelectorAll('.recharge-preset').forEach(b => {
        b.classList.toggle('btn-primary', parseInt(b.dataset.amount) === amount);
        b.classList.toggle('btn-default', parseInt(b.dataset.amount) !== amount);
    });
}
async function confirmRecharge() {
    const amount = parseFloat(document.getElementById('recharge-amount').value);
    if (!amount || amount < 1) { showError('recharge-alert', '充值金额至少1元'); return; }
    if (amount > 10000) { showError('recharge-alert', '单次充值最多10000元'); return; }
    const res = await post('/api/balance/deposit', { amount });
    if (res && res.ok) {
        hideModal('recharge-modal');
        showSuccess('vehicle-alert', '充值成功！当前余额: ¥' + parseFloat(res.data.balance).toFixed(2));
        loadBalance();
    } else showError('recharge-alert', res?.data?.error || '充值失败');
}
async function loadParkingSettings() {
    const container = document.getElementById('parking-lots-settings');
    if (!container || !hasPerm('parking.settings')) return;
    const res = await get('/api/parking/list');
    if (!res || !res.ok || !res.data.lots) { container.innerHTML = '<p style="color:#999">加载失败</p>'; return; }
    const lots = res.data.lots;
    container.innerHTML = lots.map(l => `
        <div style="display:flex;align-items:center;gap:8px;padding:8px 0;border-bottom:1px solid #f0f0f0;font-size:13px;">
            <strong style="min-width:100px;">${escapeHtml(l.P_name)}</strong>
            <span style="color:#999">${l.P_current_count}/${l.P_total_count} 占用</span>
            <input type="number" class="parking-capacity-input" value="${l.P_total_count}" min="1"
                   style="width:60px;padding:2px 6px;border:1px solid #d9d9d9;border-radius:4px;font-size:12px;"
                   data-pname="${escapeHtml(l.P_name)}" data-orig="${l.P_total_count}">
            <span style="font-size:12px;color:#999">车位</span>
            <input type="number" class="parking-fee-input" value="${parseFloat(l.P_fee).toFixed(1)}" step="0.5" min="0"
                   style="width:60px;padding:2px 6px;border:1px solid #d9d9d9;border-radius:4px;font-size:12px;"
                   data-pname="${escapeHtml(l.P_name)}" data-orig="${l.P_fee}">
            <span style="font-size:12px;color:#999">元/时</span>
            <button class="btn btn-sm btn-primary" style="padding:2px 8px;font-size:12px;display:none;" onclick="saveParkingSetting(this)">保存</button>
            <button class="btn btn-sm btn-danger" style="padding:2px 8px;font-size:12px;margin-left:auto;" onclick="deleteParkingLot(${l.P_id},'${escapeHtml(l.P_name)}')">删除</button>
        </div>`).join('') + `
        <div style="margin-top:8px;">
            <button class="btn btn-sm btn-default" onclick="showAddParkingForm()">+ 添加停车场</button>
            <div id="add-parking-form" style="display:none;margin-top:8px;display:flex;gap:8px;align-items:center;flex-wrap:wrap;">
                <input type="text" id="new-parking-name" class="form-control" placeholder="停车场名称" style="width:140px;padding:4px 8px;font-size:13px;">
                <input type="number" id="new-parking-capacity" class="form-control" value="50" min="1" style="width:70px;padding:4px 8px;font-size:13px;">
                <span style="font-size:12px;color:#999">车位</span>
                <input type="number" id="new-parking-fee" class="form-control" value="5.0" step="0.5" min="0" style="width:70px;padding:4px 8px;font-size:13px;">
                <span style="font-size:12px;color:#999">元/时</span>
                <button class="btn btn-sm btn-primary" onclick="addParkingLot()">确认添加</button>
                <button class="btn btn-sm btn-default" onclick="hideAddParkingForm()">取消</button>
            </div>
        </div>`;
        container.querySelectorAll('.parking-capacity-input, .parking-fee-input').forEach(el => {
        el.addEventListener('change', function() {
            const saveBtn = this.closest('div').querySelector('.btn-primary');
            if (saveBtn) saveBtn.style.display = 'inline-block';
        });
    });
}
async function saveParkingSetting(btn) {
    const row = btn.closest('div');
    const name = row.querySelector('strong').textContent;
    const capacity = parseInt(row.querySelector('.parking-capacity-input').value);
    const fee = parseFloat(row.querySelector('.parking-fee-input').value);
    if (capacity < 1) { showError('vehicle-alert', '车位数至少为1'); return; }
    const res = await put('/api/parking/settings', { P_name: name, P_total_count: capacity, P_fee: fee });
    if (res && res.ok) { btn.style.display = 'none'; loadParkingSettings(); }
    else showError('vehicle-alert', res?.data?.error || '保存失败');
}
async function deleteParkingLot(id, name) {
    if (!confirm('确定删除停车场 "' + name + '"？关联的计费规则和套餐也将被清理。')) return;
    const res = await del('/api/parking/lot/' + id);
    if (res && res.ok) { loadParkingSettings(); loadParkingLots(); }
    else showError('vehicle-alert', res?.data?.error || '删除失败');
}
function showAddParkingForm() {
    const form = document.getElementById('add-parking-form');
    if (form) form.style.display = 'flex';
}
function hideAddParkingForm() {
    const form = document.getElementById('add-parking-form');
    if (form) form.style.display = 'none';
}
async function addParkingLot() {
    const name = document.getElementById('new-parking-name').value.trim();
    const capacity = parseInt(document.getElementById('new-parking-capacity').value);
    const fee = parseFloat(document.getElementById('new-parking-fee').value);
    if (!name) { showError('vehicle-alert', '请输入停车场名称'); return; }
    if (capacity < 1) { showError('vehicle-alert', '车位数至少为1'); return; }
    const res = await post('/api/parking/lot', { P_name: name, P_total_count: capacity, P_fee: fee });
    if (res && res.ok) { hideAddParkingForm(); loadParkingSettings(); loadParkingLots(); }
    else showError('vehicle-alert', res?.data?.error || '添加失败');
}
document.getElementById('plate-input')?.addEventListener('keydown', e => { if (e.key === 'Enter') handleCheckIn(); });
document.getElementById('parked-search-input')?.addEventListener('keydown', e => { if (e.key === 'Enter') loadParkedVehicles(); });
applyPermUI();
initPieChart();
loadParkingLots();  
loadParkingSettings();
loadRecentRecords();
loadBalance();
loadBulletin();
loadMyBilling();
loadHeatmap();
loadPrediction();
loadInterceptionCount();
loadParkedVehicles();
loadMyPlates();
checkHealth();
setTimeout(() => { if (!currentLot) loadPassPlans(); }, 2000);
setInterval(() => { checkHealth(); }, 30000);
setInterval(() => { loadStatus(); }, 10000);
setInterval(() => { loadPrediction(); loadInterceptionCount(); }, 30000);
setInterval(() => { loadParkedVehicles(); }, 15000);
