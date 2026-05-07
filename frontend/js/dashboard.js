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
    // Only admin/operator can choose billing type — hide for regular users
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
    document.getElementById('card-settings').style.display = hasPerm('parking.settings') ? '' : 'none';
    document.getElementById('card-prediction').style.display = hasPerm('report.view') ? '' : 'none';
    document.getElementById('card-interceptions').style.display = hasPerm('vehicle.blacklist') ? '' : 'none';
    // Bulletin is visible to everyone
    document.getElementById('card-plate-recognition').style.display = hasPerm('plate.recognize') ? '' : 'none';
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
    sel.innerHTML = res.data.lots.map(l => {
        const avail = l.P_total_count - l.P_current_count - l.P_reserve_count;
        return `<option value="${escapeHtml(l.P_name)}">${escapeHtml(l.P_name)} (${avail}/${l.P_total_count})</option>`;
    }).join('');
}

async function loadStatus() {
    const res = await get('/api/parking/status');
    if (!res || !res.ok) return;
    const d = res.data;
    document.getElementById('stat-total').textContent = d.P_total_count;
    document.getElementById('stat-occupied').textContent = d.P_current_count;
    document.getElementById('stat-reserved').textContent = d.P_reserve_count;
    document.getElementById('stat-available').textContent = d.P_available_count;
    const nameEl = document.getElementById('parking-name');
    if (nameEl) nameEl.value = d.P_name;
    const feeEl = document.getElementById('parking-fee');
    if (feeEl) feeEl.value = d.P_fee;
    const capEl = document.getElementById('parking-capacity');
    if (capEl) capEl.value = d.P_total_count;
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
    const res = await get('/api/pass-plans');
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

async function handleCheckIn() {
    const plate = document.getElementById('plate-input').value.trim();
    const billingSel = document.getElementById('billing-type');
    const billingType = (billingSel && billingSel.style.display !== 'none') ? billingSel.value : 'standard';
    if (!plate) { showError('vehicle-alert', '请输入车牌号'); return; }
    const res = await post('/api/vehicle/checkin', { license_plate: plate, billing_type: billingType });
    if (res && res.ok) {
        showSuccess('vehicle-alert', `车辆 ${plate} 入库成功！`);
        document.getElementById('plate-input').value = '';
        loadStatus(); loadRecentRecords();
    } else showError('vehicle-alert', res?.data?.error || '入库失败');
}

async function handleCheckOut() {
    const plate = document.getElementById('plate-input').value.trim();
    if (!plate) { showError('vehicle-alert', '请输入车牌号'); return; }
    const res = await post('/api/vehicle/checkout', { license_plate: plate });
    if (res && res.ok) {
        showSuccess('vehicle-alert', `车辆 ${plate} 出库成功！费用: ${formatFee(res.data.fee)}。请在10分钟内驶离`);
        document.getElementById('plate-input').value = '';
        loadStatus(); loadRecentRecords(); loadBalance();
    } else showError('vehicle-alert', res?.data?.error || '出库失败');
}

async function handlePlateRecognize() {
    const res = await post('/api/plate/recognize', {});
    const c = document.getElementById('plate-result');
    if (res && res.ok) c.innerHTML = `<div class="alert alert-warning">${res.data.message}</div>`;
}

async function updateSettings() {
    const fee = parseFloat(document.getElementById('parking-fee').value);
    const capacity = parseInt(document.getElementById('parking-capacity').value);
    const res = await put('/api/parking/settings', { fee, capacity });
    if (res && res.ok) { showSuccess('vehicle-alert', '设置已更新'); loadStatus(); }
    else showError('vehicle-alert', res?.data?.error || '更新失败');
}

// ========== My Vehicles (localStorage-based) ==========
function getMyVehicles() {
    try { return JSON.parse(localStorage.getItem('my_vehicles') || '[]'); } catch { return []; }
}
function saveMyVehicles(list) {
    localStorage.setItem('my_vehicles', JSON.stringify(list));
}
async function addMyVehicle() {
    const input = document.getElementById('my-vehicle-input');
    if (!input) return;
    const plate = input.value.trim().toUpperCase();
    if (!plate) { showError('vehicle-alert', '请输入车牌号'); return; }

    // Validate plate format via API
    const valRes = await post('/api/plate/validate', { license_plate: plate });
    if (valRes && valRes.ok && !valRes.data.valid) {
        showError('my-vehicles-alert', valRes.data.message || '车牌号格式不正确');
        return;
    }

    const list = getMyVehicles();
    if (list.includes(plate)) { showError('my-vehicles-alert', '该车辆已添加'); return; }
    if (list.length >= 10) { showError('my-vehicles-alert', '最多添加10个车辆'); return; }
    list.push(plate);
    saveMyVehicles(list);
    input.value = '';
    showSuccess('my-vehicles-alert', '车辆 ' + plate + ' 已添加');
    renderMyVehicles();
}
function removeMyVehicle(plate) {
    const list = getMyVehicles().filter(v => v !== plate);
    saveMyVehicles(list);
    renderMyVehicles();
}
async function renderMyVehicles() {
    const container = document.getElementById('my-vehicles-list');
    if (!container) return;
    const list = getMyVehicles();
    if (list.length === 0) {
        container.innerHTML = '<p style="color:#999;font-size:13px">暂无车辆，请添加您的车牌号</p>';
        return;
    }

    // Fetch current status for all vehicles
    const statusRes = await get('/api/vehicle/status');
    const statusMap = {};
    if (statusRes && statusRes.ok && statusRes.data.vehicles) {
        statusRes.data.vehicles.forEach(v => { statusMap[v.license_plate] = v; });
    }

    const canCheckin = hasPerm('vehicle.checkin');
    const canCheckout = hasPerm('vehicle.checkout');

    container.innerHTML = list.map(plate => {
        const info = statusMap[plate];
        const isParked = info && info.is_parked;
        const lastIn = info ? info.last_check_in : null;
        return `<div style="display:flex;justify-content:space-between;align-items:center;padding:6px 8px;border-bottom:1px solid #f0f0f0;font-size:13px">
            <div>
                <strong>${escapeHtml(plate)}</strong>
                ${isParked
                    ? '<span class="badge badge-primary" style="margin-left:6px">停放中</span>'
                    : '<span class="badge badge-success" style="margin-left:6px">空闲</span>'}
                ${lastIn ? '<span style="color:#bbb;font-size:11px;margin-left:6px">' + formatDateTime(lastIn) + '</span>' : ''}
            </div>
            <div>
                ${isParked && canCheckout
                    ? '<button class="btn btn-danger btn-xs" onclick="quickCheckOut(\'' + plate + '\')">出库</button>'
                    : !isParked && canCheckin
                    ? '<button class="btn btn-primary btn-xs" onclick="quickCheckIn(\'' + plate + '\')">入库</button>'
                    : ''}
                <button class="btn btn-default btn-xs" onclick="removeMyVehicle('${plate}')" title="移除" style="margin-left:4px">&times;</button>
            </div>
        </div>`;
    }).join('');
}

async function quickCheckIn(plate) {
    const res = await post('/api/vehicle/checkin', { license_plate: plate, billing_type: 'standard' });
    if (res && res.ok) {
        showSuccess('my-vehicles-alert', '车辆 ' + plate + ' 入库成功！');
        renderMyVehicles(); loadStatus(); loadRecentRecords();
    } else showError('my-vehicles-alert', res?.data?.error || '入库失败');
}

async function quickCheckOut(plate) {
    const res = await post('/api/vehicle/checkout', { license_plate: plate });
    if (res && res.ok) {
        showSuccess('my-vehicles-alert', '车辆 ' + plate + ' 出库成功！费用: ' + formatFee(res.data.fee));
        renderMyVehicles(); loadStatus(); loadRecentRecords(); loadBalance();
    } else showError('my-vehicles-alert', res?.data?.error || '出库失败');
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

document.getElementById('plate-input')?.addEventListener('keydown', e => { if (e.key === 'Enter') handleCheckIn(); });
document.getElementById('my-vehicle-input')?.addEventListener('keydown', e => { if (e.key === 'Enter') addMyVehicle(); });

// Show my-vehicles card for all authenticated users
const myVehiclesCard = document.getElementById('card-my-vehicles');
if (myVehiclesCard) myVehiclesCard.style.display = '';

// Init
applyPermUI();
initPieChart();
loadParkingLots();
loadStatus();
loadRecentRecords();
loadBalance();
loadPassPlans();
loadBulletin();
loadPrediction();
loadInterceptionCount();
renderMyVehicles();
setInterval(() => { loadStatus(); }, 10000);
setInterval(() => { renderMyVehicles(); loadPrediction(); loadInterceptionCount(); }, 30000);
