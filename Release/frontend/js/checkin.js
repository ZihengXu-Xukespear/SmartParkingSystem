const user = checkAuth();
if (user) { initSidebar(); if (!hasPerm('vehicle.checkin')) { document.querySelector('.main-content').innerHTML = '<div class="card" style="text-align:center;padding:60px"><h2>权限不足</h2><p style="color:#999;">需要车辆入库权限</p></div>'; } }

let allLots = [], currentLotName = '', selectedSpot = 0;

async function loadLots() {
    const res = await get('/api/parking/list');
    if (!res || !res.ok) return;
    allLots = res.data.lots || [];
    const sel = document.getElementById('lot-selector');
    if (!sel || allLots.length === 0) return;
    sel.innerHTML = allLots.map(l =>
        `<option value="${escapeHtml(l.P_name)}">${escapeHtml(l.P_name)}</option>`
    ).join('');
    sel.value = allLots[0].P_name;
    switchToLot(allLots[0].P_name);
}

function onLotChange() {
    const sel = document.getElementById('lot-selector');
    if (sel) switchToLot(sel.value);
}

function switchToLot(name) {
    const lot = allLots.find(l => l.P_name === name);
    if (!lot) return;
    currentLotName = name;
    selectedSpot = 0;
    document.getElementById('selected-spot-label').textContent = '';
    document.getElementById('stat-total').textContent = lot.P_total_count;
    document.getElementById('stat-occupied').textContent = lot.P_current_count;
    document.getElementById('stat-reserved').textContent = lot.P_reserve_count;
    document.getElementById('stat-available').textContent = lot.P_total_count - lot.P_current_count - lot.P_reserve_count;
    loadSpotMap();
    loadBillingTypes();
}

async function loadBillingTypes() {
    if (!currentLotName) return;
    const res = await get('/api/parking/billing-types?P_name=' + encodeURIComponent(currentLotName));
    const sel = document.getElementById('checkin-billing');
    if (!sel) return;
    sel.innerHTML = '';
    if (res && res.ok && res.data.types && res.data.types.length > 0) {
        res.data.types.forEach(t => {
            const opt = document.createElement('option');
            opt.value = t.rule_type;
            opt.textContent = t.rule_name;
            sel.appendChild(opt);
        });
    } else {
        sel.innerHTML = '<option value="standard">标准计费</option>';
    }
}

async function loadSpotMap() {
    if (!currentLotName) return;
    const res = await get('/api/reservation/spots?P_name=' + encodeURIComponent(currentLotName));
    if (!res || !res.ok) return;
    const spots = res.data.spots || [];
    const container = document.getElementById('spot-map');
    container.innerHTML = spots.map(s => {
        let cls = 'spot ' + s.status;
        let label = s.status === 'available' ? '空闲' : s.status === 'occupied' ? '占用' : '预约';
        let click = s.status === 'available' ? `onclick="selectSpot(${s.number})"` : '';
        return `<div class="${cls}" id="spot-${s.number}" ${click}>
            <span>${s.number}</span><span style="font-size:9px;opacity:0.8;">${label}</span></div>`;
    }).join('');
}

function selectSpot(num) {
    document.querySelectorAll('#spot-map .spot.selected').forEach(d => d.classList.remove('selected'));
    const el = document.getElementById('spot-' + num);
    if (el) el.classList.add('selected');
    selectedSpot = num;
    document.getElementById('selected-spot-label').textContent = '已选 ' + num + ' 号车位';
}

async function doCheckIn() {
    const plate = document.getElementById('checkin-plate').value.trim().toUpperCase();
    const billing = document.getElementById('checkin-billing').value;
    if (!plate) { showError('alert-box', '请输入车牌号'); return; }
    if (!currentLotName) { showError('alert-box', '请选择停车场'); return; }
    if (selectedSpot <= 0) { showError('alert-box', '请选择车位'); return; }

    const body = { license_plate: plate, billing_type: billing, P_name: currentLotName, spot_number: selectedSpot };

    const res = await post('/api/vehicle/checkin', body);
    if (res && res.ok) {
        showSuccess('alert-box', `车辆 ${plate} 已入库${selectedSpot ? ' (' + selectedSpot + '号车位)' : ''}`);
        document.getElementById('checkin-plate').value = '';
        selectedSpot = 0;
        document.getElementById('selected-spot-label').textContent = '';
        loadSpotMap();
        // Reload lot stats
        const lotRes = await get('/api/parking/list');
        if (lotRes && lotRes.ok) allLots = lotRes.data.lots || [];
    } else {
        showError('alert-box', res?.data?.error || '入库失败');
    }
}

document.getElementById('checkin-plate')?.addEventListener('keydown', e => { if (e.key === 'Enter') doCheckIn(); });
loadLots();
