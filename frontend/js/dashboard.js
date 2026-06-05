// Dashboard logic
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
    // Checkout moved to admin > vehicle management — always hide on dashboard
    const btnCO = document.getElementById('btn-checkout');
    if (btnCO) btnCO.style.display = 'none';
    // Only admin can choose billing type — hide for regular users
    const billingSel = document.getElementById('billing-type');
    const billingLabel = billingSel?.parentElement?.querySelector('label');
    if (user.role === 'user' && billingSel) {
        billingSel.style.display = 'none';
        if (billingLabel) billingLabel.style.display = 'none';
    }
    const btnPlate = document.getElementById('btn-plate-recognize');
    if (btnPlate) btnPlate.style.display = hasPerm('plate.recognize') ? '' : 'none';

    document.getElementById('card-pass-plans').style.display = hasPerm('balance.view') ? '' : 'none';
    document.getElementById('card-balance').style.display = hasPerm('balance.view') ? '' : 'none';
    document.getElementById('card-recent-records').style.display = hasPerm('vehicle.query') ? '' : 'none';
    document.getElementById('card-prediction').style.display = hasPerm('report.view') ? '' : 'none';
    document.getElementById('card-interceptions').style.display = hasPerm('vehicle.blacklist') ? '' : 'none';
    document.getElementById('card-plate-recognition').style.display = hasPerm('plate.recognize') ? '' : 'none';
    document.getElementById('card-parked-vehicles').style.display = hasPerm('vehicle.query') ? '' : 'none';
    document.getElementById('card-settings').style.display = hasPerm('parking.settings') ? '' : 'none';
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
    // Default to first lot if not yet selected
    if (lots.length > 0 && !sel.dataset.loaded) {
        sel.dataset.loaded = '1';
        currentLot = lots[0].P_name;
        sel.value = currentLot;
    }
    // Also update the checkin form's lot options
    const checkinLot = document.getElementById('checkin-lot');
    if (checkinLot) {
        checkinLot.innerHTML = lots.map(l =>
            `<option value="${escapeHtml(l.P_name)}">${escapeHtml(l.P_name)}</option>`
        ).join('');
    }
    // Now that currentLot is set, load status and plans with the correct lot
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
}

async function loadRecentRecords() {
    const tbody = document.getElementById('recent-records');
    if (!tbody || !hasPerm('vehicle.query')) return;
    const res = await get('/api/vehicle/query');
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
            <td><span style="color:${isOvertime?'#ff4d4f':'#666'};font-weight:${isOvertime?'600':'400'}">${r.duration || '计算中...'}${isOvertime?' ⚠️超24h':''}</span></td>
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
                </div>`
            ).join('') || '<p style="color:#999;font-size:12px">暂无交易记录</p>';
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
        container.innerHTML = '<p style="color:#999">暂无可用套餐</p>';
        return;
    }
    container.innerHTML = res.data.plans.map(p => `
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

// Purchase modal logic
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

    // Validate plate format
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
    } else {
        showError('purchase-alert', res?.data?.error || '购买失败');
    }
}

// ========== Demo Mode + Receipt ==========
const demoProvinces = ['京','沪','粤','苏','浙','鲁','川','渝','闽','皖','湘','鄂'];
const demoLetters = 'ABCDEFGHJKLMNPQRSTUVWXYZ';
let demoRunning = false;
let demoStop = false;

function stopDemo() {
    demoStop = true;
    document.getElementById('demo-hint').innerHTML = '<span style="color:#ff4d4f;">⏹ 演示已停止</span>';
    document.getElementById('btn-stop-demo').style.display = 'none';
    document.getElementById('btn-demo').textContent = '▶ 演示模式';
    document.getElementById('btn-demo').disabled = false;
    // Remove any glows
    document.querySelectorAll('.demo-glow').forEach(el => el.classList.remove('demo-glow'));
    document.querySelectorAll('.demo-glow-btn').forEach(el => el.classList.remove('demo-glow-btn'));
}

function randomPlate() {
    let plate = demoProvinces[Math.floor(Math.random() * demoProvinces.length)];
    plate += demoLetters[Math.floor(Math.random() * demoLetters.length)];
    for (let i = 0; i < 5; i++) plate += Math.floor(Math.random() * 10);
    return plate;
}

function setDot(idx, status) {
    const dots = document.querySelectorAll('.demo-step-dot');
    const colors = { active: '#1890ff', done: '#52c41a', fail: '#ff4d4f' };
    dots.forEach((d, i) => {
        if (i < idx) { d.style.background = '#52c41a'; d.style.color = '#fff'; }
        else if (i === idx) { d.style.background = colors[status] || '#1890ff'; d.style.color = '#fff'; }
        else { d.style.background = '#eee'; d.style.color = '#999'; }
    });
}

function glow(el, hint, color) {
    const c = color || '#1890ff';
    el.classList.add('demo-glow');
    el.style.borderColor = c;
    document.getElementById('demo-hint').innerHTML = `<span style="color:${c};">👉 ${hint}</span>`;
    el.scrollIntoView({ behavior: 'smooth', block: 'center' });
}

function unglow(el) {
    el.classList.remove('demo-glow');
    el.style.borderColor = '';
}

function simulateClick(el) {
    el.style.transform = 'scale(0.95)';
    el.style.transition = 'all 0.1s';
    setTimeout(() => { el.style.transform = ''; }, 150);
}

async function startDemo() {
    if (demoRunning) return;
    demoRunning = true;
    demoStop = false;
    const btn = document.getElementById('btn-demo');
    const steps = document.getElementById('demo-steps');
    const oldText = btn.textContent;
    btn.textContent = '⏳ 演示中...';
    btn.disabled = true;
    steps.style.display = 'block';
    document.getElementById('demo-hint').textContent = '';
    document.getElementById('btn-stop-demo').style.display = 'inline-block';

    const plate = randomPlate();
    const input = document.getElementById('plate-input');

    // ── Step 1: Fill plate ──
    setDot(0, 'active');
    glow(input, `正在生成车牌... ${plate}`);
    await new Promise(r => setTimeout(r, 1200));
    if (demoStop) { cleanupDemo(); return; }
    input.value = plate;
    input.style.borderColor = '#52c41a';
    input.style.transition = 'border-color 0.3s';
    setDot(0, 'done');
    await new Promise(r => setTimeout(r, 400));
    if (demoStop) { cleanupDemo(); return; }

    // ── Step 2: Click check-in ──
    const btnIn = document.getElementById('btn-checkin');
    setDot(1, 'active');
    unglow(input);
    input.style.borderColor = '';
    glow(btnIn, '点击"快速入库"按钮');
    await new Promise(r => setTimeout(r, 1000));
    if (demoStop) { cleanupDemo(); return; }
    simulateClick(btnIn);
    document.getElementById('demo-hint').innerHTML = '<span style="color:#52c41a;">⏳ 正在请求入库...</span>';
    let res = await post('/api/vehicle/checkin', { license_plate: plate, billing_type: 'standard' });
    if (!res || !res.ok) { setDot(1, 'fail'); document.getElementById('demo-hint').innerHTML = '<span style="color:#ff4d4f;">❌ 入库失败: ' + (res?.data?.error||'') + '</span>'; cleanupDemo(); return; }
    unglow(btnIn);
    setDot(1, 'done');
    document.getElementById('demo-hint').innerHTML = '<span style="color:#52c41a;">✅ 入库成功！车辆已入场</span>';
    await new Promise(r => setTimeout(r, 800));
    if (demoStop) { cleanupDemo(); return; }

    // ── Step 3: Parking countdown ──
    setDot(2, 'active');
    document.getElementById('demo-hint').innerHTML = '<span style="color:#1890ff;">⏱ 车辆停放中...</span>';
    for (let i = 3; i > 0; i--) {
        document.getElementById('demo-hint').innerHTML = `<span style="color:#1890ff;">⏱ 停车计时 ${i}秒...</span>`;
        await new Promise(r => setTimeout(r, 1000));
        if (demoStop) { cleanupDemo(); return; }
    }
    setDot(2, 'done');
    document.getElementById('demo-hint').innerHTML = '<span style="color:#52c41a;">✅ 停车结束</span>';
    await new Promise(r => setTimeout(r, 400));
    if (demoStop) { cleanupDemo(); return; }

    // ── Step 4: Click check-out ──
    const btnOut = document.getElementById('btn-checkout');
    setDot(3, 'active');
    glow(btnOut, '点击"车辆出库"按钮', '#ff4d4f');
    await new Promise(r => setTimeout(r, 1000));
    if (demoStop) { cleanupDemo(); return; }
    simulateClick(btnOut);
    document.getElementById('demo-hint').innerHTML = '<span style="color:#ff4d4f;">⏳ 正在出库计费...</span>';
    res = await post('/api/vehicle/checkout', { license_plate: plate });
    if (!res || !res.ok) { setDot(3, 'fail'); document.getElementById('demo-hint').innerHTML = '<span style="color:#ff4d4f;">❌ 出库失败: ' + (res?.data?.error||'') + '</span>'; cleanupDemo(); return; }
    const data = res.data;
    const fee = data.fee || 0;
    unglow(btnOut);
    setDot(3, 'done');
    document.getElementById('demo-hint').innerHTML = '<span style="color:#52c41a;">✅ 出库成功，费用: ¥' + parseFloat(fee).toFixed(2) + '</span>';
    await new Promise(r => setTimeout(r, 600));

    // ── Step 5: Receipt ──
    setDot(4, 'active');
    document.getElementById('demo-hint').innerHTML = '<span style="color:#1890ff;">📄 正在生成电子小票...</span>';
    await new Promise(r => setTimeout(r, 600));
    showReceipt(plate, data);
    setDot(4, 'done');
    document.getElementById('demo-hint').innerHTML = '<span style="color:#52c41a;">✅ 演示完成！系统导览已就绪</span>';

    loadRecentRecords();
    loadBalance();
    loadParkedVehicles();

    // Sidebar tour
    await new Promise(r => setTimeout(r, 1200));
    hideModal('receipt-modal');
    if (!demoStop) await sidebarTour();
    cleanupDemo();
}

function cleanupDemo() {
    document.getElementById('btn-stop-demo').style.display = 'none';
    demoRunning = false;
    document.getElementById('btn-demo').textContent = '▶ 演示模式';
    document.getElementById('btn-demo').disabled = false;
    document.querySelectorAll('.demo-glow, .demo-glow-btn').forEach(el => {
        el.classList.remove('demo-glow', 'demo-glow-btn');
        el.style.borderColor = '';
        el.style.transform = '';
    });
}

// ========== Sidebar Tour ==========
const tourPages = [
    { id: 'nav-dashboard', label: '主面板', desc: '停车场实时状态、快捷出入库、收入预测、套餐购买' },
    { id: 'nav-parking', label: '停车场', desc: '多停车场管理、车位数与费率设置' },
    { id: 'nav-checkin', label: '车辆入库', desc: '选择停车场和车位，手动入库' },
    { id: 'nav-vehicles', label: '车辆信息', desc: '查询出入记录、在场车辆管理' },
    { id: 'nav-admin', label: '管理页面', desc: '用户管理、套餐、计费规则、黑名单、消息管理' },
    { id: 'nav-reservation', label: '预约管理', desc: '车位预约与历史预约查询' },
    { id: 'nav-recognize', label: '车牌识别', desc: '摄像头抓拍自动识别车牌' },
    { id: 'nav-chat', label: '联系客服', desc: '在线客服消息咨询' },
];

async function sidebarTour() {
    const hint = document.getElementById('demo-hint');
    const steps = document.getElementById('demo-steps');
    for (const page of tourPages) {
        if (demoStop) return;
        const el = document.getElementById(page.id);
        if (!el) continue;
        el.classList.add('demo-glow');
        hint.innerHTML = `<span style="color:#1890ff;">👉 ${page.label}：${page.desc} <span style="font-size:11px;color:#999;">（点击可跳转）</span></span>`;

        // Wait for user click OR timeout
        const clicked = await new Promise(resolve => {
            const handler = (e) => { e.preventDefault(); el.removeEventListener('click', handler); resolve(true); };
            el.addEventListener('click', handler);
            setTimeout(() => { el.removeEventListener('click', handler); resolve(false); }, 2500);
        });
        el.classList.remove('demo-glow');

        if (clicked) {
            // Navigate to that page with demo flag
            const href = el.getAttribute('href');
            if (href) {
                const sep = href.includes('?') ? '&' : '?';
                window.location.href = href + sep + 'demo=1';
                await new Promise(() => {}); // never resolves (navigation)
            }
        }
        await new Promise(r => setTimeout(r, 300));
    }
    if (!demoStop) {
        hint.innerHTML = '<span style="color:#52c41a;">✅ 系统导览结束，你可以自由探索各个功能页面了</span>';
        steps.style.display = 'none';
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
        // Render each bulletin with markdown support
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

// ========== Smart Report ==========
async function showSmartReport() {
    showModal('report-modal');
    document.getElementById('report-date').textContent = '报告生成: ' + new Date().toLocaleString('zh-CN');
    document.getElementById('report-body').innerHTML = '<div style="color:rgba(255,255,255,0.6);padding:40px 0;">⏳ 分析中...</div>';

    const [statusRes, summaryRes, parkedRes, hourlyRes] = await Promise.all([
        get('/api/parking/status'),
        get('/api/report/summary'),
        get('/api/vehicle/parked'),
        get('/api/report/hourly')
    ]);

    const total = statusRes?.data?.P_total_count || 100;
    const occupied = statusRes?.data?.P_current_count || 0;
    const available = statusRes?.data?.P_available_count || 0;
    const occRate = total > 0 ? (occupied / total * 100).toFixed(0) : '0';
    const todayIncome = parseFloat(summaryRes?.data?.today_income || 0).toFixed(2);
    const monthIncome = parseFloat(summaryRes?.data?.month_income || 0).toFixed(0);
    const parkedCount = parkedRes?.data?.records?.length || 0;

    // Find peak hour
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
    document.getElementById('receipt-duration').textContent = duration || '计算中...';
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

// ========== Recharge ==========
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

// ========== Parking Settings (Dashboard) ==========
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
    // Show save buttons on input change
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

// Init
applyPermUI();
initPieChart();
loadParkingLots();  // calls loadStatus() + loadPassPlans() after setting currentLot
loadParkingSettings();
loadRecentRecords();
loadBalance();
loadBulletin();
loadHeatmap();
loadPrediction();
loadInterceptionCount();
loadParkedVehicles();
setInterval(() => { loadStatus(); }, 10000);
setInterval(() => { loadPrediction(); loadInterceptionCount(); }, 30000);
setInterval(() => { loadParkedVehicles(); }, 15000);
