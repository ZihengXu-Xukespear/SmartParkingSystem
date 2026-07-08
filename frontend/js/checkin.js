const user = checkAuth();
if (user) { initSidebar(); if (!hasPerm('vehicle.checkin')) { document.querySelector('.main-content').innerHTML = '<div class="card" style="text-align:center;padding:60px"><h2>权限不足</h2><p style="color:#999;">需要车辆入库权限</p></div>'; } }
let allLots = [], currentLotName = '', selectedSpot = 0;
function isEV(n) { return (n - 1) % 25 >= 20; }
async function loadLots() {
    const res = await get('/api/parking/list');
    if (!res || !res.ok) return;
    allLots = res.data.lots || [];
    const sel = document.getElementById('lot-selector');
    if (!sel || allLots.length === 0) return;
    sel.innerHTML = allLots.map(l =>
        `<option value="${escapeHtml(l.P_name)}">${escapeHtml(l.P_name)}</option>`    ).join('');
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
    const perZone = 25, cols = 5;
    const zNames = ['A区','B区','C区','D区'];
    const zColors = ['#e3f2fd','#fff8e1','#e8f5e9','#fce4ec'];
        const statusMap = {};
    spots.forEach(s => { statusMap[s.number] = s.status; });
    function zoneHtml(zoneIdx, zoneStart) {
        let h = '<div style="flex:1;background:'+zColors[zoneIdx]+';border-radius:4px;padding:4px;border:1px solid #e0e0e0;'+(zoneIdx%2===0?'margin-right:4px;':'margin-left:4px;')+'">';
        h += '<div style="text-align:center;font-size:11px;font-weight:700;color:#555;margin-bottom:2px;">'+zNames[zoneIdx]+'</div>';
        for (let r = 0; r < cols; r++) {
            h += '<div style="display:flex;gap:3px;justify-content:center;">';
            for (let c = 0; c < cols; c++) {
                const n = zoneStart + r * cols + c + 1;
                if (n > 100) { h += '<div style="width:46px;height:46px;"></div>'; continue; }
                const st = statusMap[n] || 'available';
                const sel = n === selectedSpot;
                const ev = isEV(n);
                const extra = (ev && st === 'available') ? ' ev' : '';
                const cls = sel ? 'fp-spot selected' : 'fp-spot ' + st + extra;
                const click = st === 'available' ? ' onclick="selectSpot('+n+')"' : '';
                h += '<div class="'+cls+'"'+click+' style="width:46px;height:46px;font-size:10px;">'+n+'</div>';
            }
            h += '</div>';
        }
        h += '</div>';
        return h;
    }
    let html = '<div style="display:inline-block;background:#fafafa;border-radius:8px;padding:12px;">';
        html += '<div style="display:flex;">';
    html += zoneHtml(0, 0);
    html += zoneHtml(1, perZone);
    html += '</div>';
        html += '<div style="display:flex;align-items:center;margin:4px 0;"><div style="flex:1;height:28px;background:#e0e0e0;border-radius:2px;display:flex;align-items:center;justify-content:center;"><span style="font-size:10px;color:#888;">🚗 入口 ➡  ⸺⸺  主干道  ⸺⸺  ➡ 🚗 出口</span></div></div>';
        html += '<div style="display:flex;">';
    html += zoneHtml(2, perZone*2);
    html += zoneHtml(3, perZone*3);
    html += '</div>';
    html += '</div>';
    container.innerHTML = html;
}
function zoneLabel(num) {
    if (!num || num < 1) return '';
    if (num <= 25) return 'A区' + num;
    if (num <= 50) return 'B区' + num;
    if (num <= 75) return 'C区' + num;
    return 'D区' + num;
}
function selectSpot(num) {
    document.querySelectorAll('#spot-map .fp-spot.selected').forEach(d => d.classList.remove('selected'));
    loadSpotMap();
    setTimeout(() => {
        document.querySelectorAll('#spot-map .fp-spot').forEach(d => {
            if (d.textContent.trim() === String(num)) d.classList.add('selected');
        });
    }, 50);
    selectedSpot = num;
    document.getElementById('selected-spot-label').textContent = '已选 ' + zoneLabel(num);
    document.getElementById('charging-section').style.display = isEV(num) ? 'block' : 'none';
}
async function loadReservations() {
    const container = document.getElementById('reservation-list');
    if (!container) return;
    const res = await get('/api/reservation/list');
    if (!res || !res.ok || !res.data.reservations) { container.innerHTML = '<span>暂无待入库预约</span>'; return; }
    const list = res.data.reservations.filter(r => r.status === 'active');
    if (list.length === 0) { container.innerHTML = '<span>暂无待入库预约</span>'; return; }
    container.innerHTML = list.map(r => `
        <div style="display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid #f0f0f0;">
            <div><strong>${r.license_plate}</strong> <span style="color:#999;font-size:12px;">${r.P_name||''} ${r.spot_number ? '· '+r.spot_number+'号':''}</span></div>
            <button class="btn btn-primary btn-sm" onclick="checkinFromReservation('${r.license_plate}',${r.spot_number||0},'${r.P_name||''}')">一键入库</button>
        </div>
    `).join('');
}
async function checkinFromReservation(plate, spotNum, pname) {
    if (!plate) return;
    const body = { license_plate: plate, billing_type: 'standard', P_name: pname || currentLotName, spot_number: spotNum };
    const res = await post('/api/vehicle/checkin', body);
    if (res && res.ok) {
        showSuccess('alert-box', `<div style="font-size:15px;font-weight:600;">✅ 预约车辆已入库</div><div>🚗 ${plate} → ${zoneLabel(spotNum)}</div>`);
        loadReservations();
        switchToLot(currentLotName);
    } else {
        showError('alert-box', res?.data?.error || '入库失败');
    }
}
async function doCheckIn() {
    const plate = document.getElementById('checkin-plate').value.trim().toUpperCase();
    const billing = document.getElementById('checkin-billing').value;
    if (!plate) { showError('alert-box', '请输入车牌号'); return; }
    if (!currentLotName) { showError('alert-box', '请选择停车场'); return; }
    if (selectedSpot <= 0) { showError('alert-box', '请选择车位'); return; }
    const chargePlan = document.getElementById('charging-plan')?.value || '';
    const body = { license_plate: plate, billing_type: billing, P_name: currentLotName, spot_number: selectedSpot };
    if (chargePlan) body.charging_plan = chargePlan;
    const res = await post('/api/vehicle/checkin', body);
    if (res && res.ok) {
        const spotInfo = zoneLabel(selectedSpot);
        let chargeInfo = '';
        const chargePrices = {charge_1h:'¥5.00/时', charge_3h:'¥12.00/3时', charge_6h:'¥20.00/6时', charge_12h:'¥35.00/12时'};
        if (chargePlan && chargePrices[chargePlan]) chargeInfo = ' | ⚡充电: ' + chargePrices[chargePlan];
        showSuccess('alert-box', `<div style="text-align:left;line-height:1.8;">
            <div style="font-size:15px;font-weight:600;margin-bottom:4px;">✅ 入库成功</div>
            <div>🚗 ${plate} → ${spotInfo}</div>
            <div>📋 ${currentLotName} | ${billing}</div>${chargeInfo ? '<div>'+chargeInfo+'</div>' : ''}
        </div>`);
        document.getElementById('checkin-plate').value = '';
        selectedSpot = 0;
        document.getElementById('selected-spot-label').textContent = '';
        loadSpotMap();
        const lotRes = await get('/api/parking/list');
        if (lotRes && lotRes.ok) allLots = lotRes.data.lots || [];
    } else {
        showError('alert-box', res?.data?.error || '入库失败');
    }
}
document.getElementById('checkin-plate')?.addEventListener('keydown', e => { if (e.key === 'Enter') doCheckIn(); });
loadLots();
loadReservations();
