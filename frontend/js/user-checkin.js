
const user = checkAuth();
if (user) initSidebar();
let currentLotName = '';
let pendingPlate = '';
let allLots = [];
let cameraStream = null;
async function loadLots() {
    const res = await get('/api/parking/list');
    if (!res || !res.ok || !res.data.lots) return;
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
    const avail = lot.P_total_count - lot.P_current_count - lot.P_reserve_count;
    document.getElementById('lot-available').textContent = avail;
    document.getElementById('lot-available').style.color = avail <= 0 ? '#ff4d4f' : avail <= 5 ? '#faad14' : '#52c41a';
    loadSpotMap();
}
function isEVSpot(n) { return (n - 1) % 25 >= 20; }
async function loadSpotMap() {
    const container = document.getElementById('spot-map');
    if (!container || !currentLotName) return;
    const res = await get('/api/reservation/spots?P_name=' + encodeURIComponent(currentLotName));
    if (!res || !res.ok) { container.innerHTML = '<p style="color:#999;font-size:13px">暂无数据</p>'; return; }
    const spots = res.data.spots || [];
    const statusMap = {};
    spots.forEach(s => { statusMap[s.number] = s.status; });
    const perZone = 25, cols = 5;
    const zNames = ['A区','B区','C区','D区'];
    const zColors = ['#e3f2fd','#fff8e1','#e8f5e9','#fce4ec'];
    function zoneHtml(zoneIdx, zoneStart) {
        let h = '<div style="flex:1;background:'+zColors[zoneIdx]+';border-radius:4px;padding:4px;border:1px solid #e0e0e0;'+(zoneIdx%2===0?'margin-right:4px;':'margin-left:4px;')+'">';
        h += '<div style="text-align:center;font-size:11px;font-weight:700;color:#555;margin-bottom:2px;">'+zNames[zoneIdx]+'</div>';
        for (let r = 0; r < cols; r++) {
            h += '<div style="display:flex;gap:3px;justify-content:center;">';
            for (let c = 0; c < cols; c++) {
                const n = zoneStart + r * cols + c + 1;
                if (n > 100) { h += '<div style="width:46px;height:46px;"></div>'; continue; }
                const st = statusMap[n] || 'available';
                const ev = isEVSpot(n);
                                let cls = 'fp-spot ';
                if (ev) cls += 'ev ';
                cls += st;
                h += '<div class="'+cls+'" style="width:46px;height:46px;font-size:10px;">'+n+'</div>';
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
async function startCamera() {
    try {
        cameraStream = await navigator.mediaDevices.getUserMedia({
            video: { facingMode: 'environment', width: { ideal: 1280 }, height: { ideal: 720 } }
        });
        const video = document.getElementById('camera-preview');
        video.srcObject = cameraStream;
        video.style.display = 'block';
        document.getElementById('camera-placeholder').style.display = 'none';
        document.getElementById('btn-capture').disabled = false;
        document.getElementById('btn-stop-camera').disabled = false;
        document.getElementById('btn-start-camera').disabled = true;
    } catch (e) {
        showError('alert-box', '无法访问摄像头: ' + e.message);
    }
}
function stopCamera() {
    if (cameraStream) {
        cameraStream.getTracks().forEach(t => t.stop());
        cameraStream = null;
    }
    document.getElementById('camera-preview').style.display = 'none';
    document.getElementById('camera-placeholder').style.display = 'block';
    document.getElementById('btn-capture').disabled = true;
    document.getElementById('btn-stop-camera').disabled = true;
    document.getElementById('btn-start-camera').disabled = false;
}
async function captureAndRecognize() {
    const video = document.getElementById('camera-preview');
    const canvas = document.getElementById('camera-canvas');
    canvas.width = video.videoWidth || 640;
    canvas.height = video.videoHeight || 480;
    canvas.getContext('2d').drawImage(video, 0, 0);
    const dataURL = canvas.toDataURL('image/jpeg', 0.9);
    showError('alert-box', '正在识别中...');
    try {
        const res = await post('/api/plate/recognize-image', { image: dataURL });
        if (res && res.ok && res.data.plate_number) {
            const plate = res.data.plate_number;
            processRecognizedPlate(plate, res.data);
        } else {
            showError('alert-box', (res && res.data && res.data.message) || '未识别到车牌，请重试');
        }
    } catch (e) {
        showError('alert-box', '识别失败: ' + e.message);
    }
}
async function manualCheckin() {
    const plate = document.getElementById('manual-plate').value.trim().toUpperCase();
    if (!plate) { showError('alert-box', '请输入车牌号'); return; }
        const valRes = await post('/api/plate/validate', { license_plate: plate });
    if (valRes && valRes.ok && !valRes.data.valid) {
        showError('alert-box', valRes.data.message || '车牌号格式不正确');
        return;
    }
    processRecognizedPlate(plate, { plate_number: plate, confidence: '手动输入', color: '-' });
}
async function processRecognizedPlate(plate, recognizeData) {
    pendingPlate = plate;
        document.getElementById('result-placeholder').style.display = 'none';
    document.getElementById('result-content').style.display = 'block';
    document.getElementById('result-plate').textContent = plate;
    document.getElementById('result-alert').innerHTML = '';
    document.getElementById('checkin-result').innerHTML = '';
    document.getElementById('result-actions').style.display = 'none';
    document.getElementById('btn-confirm-checkin').style.display = 'none';
        const regRes = await post('/api/plate/check-registered', { license_plate: plate });
    const reg = regRes && regRes.ok ? regRes.data : null;
        renderRegistrationInfo(plate, reg, recognizeData);
        if (reg && reg.is_blacklisted) {
                document.getElementById('checkin-result').innerHTML =
            '<div class="result-card checkin-error"><p style="color:#721c24;font-weight:600;margin:0">🚫 该车辆已被列入黑名单，禁止入库</p></div>';
        addHistory(plate, '黑名单拦截');
        return;
    }
    if (reg && reg.in_parking) {
                document.getElementById('checkin-result').innerHTML =
            '<div class="result-card checkin-warning"><p style="color:#856404;font-weight:600;margin:0">⚠️ 该车辆已在停车场内</p></div>';
        addHistory(plate, '已在场内');
        return;
    }
        const hasReservation = await checkReservation(plate);
        const isBoundPlate = await checkBoundPlate(plate);
    if (hasReservation) {
                showSuccess('alert-box', '检测到预约车辆，正在自动入库...');
        await doCheckin(plate, true);
    } else if (isBoundPlate) {
                showSuccess('alert-box', '检测到您的绑定车辆，正在自动入库...');
        await doCheckin(plate, true);
    } else {
                const lot = allLots.find(l => l.P_name === currentLotName);
        const avail = lot ? lot.P_total_count - lot.P_current_count - lot.P_reserve_count : 0;
        if (avail <= 0) {
            document.getElementById('checkin-result').innerHTML =
                '<div class="result-card checkin-error"><p style="color:#721c24;font-weight:600;margin:0">🚫 停车场已满，无法入库</p></div>';
            addHistory(plate, '停车场已满');
            return;
        }
                document.getElementById('checkin-result').innerHTML =
            '<div class="result-card checkin-warning"><p style="color:#856404;margin:0">该车辆无预约记录。空余车位：<strong>' + avail + '</strong></p></div>';
        document.getElementById('result-actions').style.display = 'block';
        document.getElementById('btn-confirm-checkin').style.display = 'block';
    }
}
async function checkReservation(plate) {
    const res = await get('/api/reservation/list');
    if (!res || !res.ok || !res.data.reservations) return false;
    return res.data.reservations.some(r => r.license_plate === plate && r.status === 'active');
}
async function checkBoundPlate(plate) {
    const res = await get('/api/user/plates');
    if (!res || !res.ok || !res.data.plates) return false;
    return res.data.plates.some(p => p.license_plate === plate);
}
async function confirmCheckin() {
        document.getElementById('confirm-plate').textContent = pendingPlate;
    showModal('confirm-modal');
}
async function doNonReservedCheckin() {
    hideModal('confirm-modal');
    await doCheckin(pendingPlate, false);
}
async function doCheckin(plate, isReserved) {
    const body = {
        license_plate: plate,
        billing_type: 'standard'
    };
    if (currentLotName) body.P_name = currentLotName;
    const res = await post('/api/vehicle/checkin', body);
    if (res && res.ok) {
        const label = isReserved ? '预约车辆自动入库' : '非预约车辆入库';
        document.getElementById('checkin-result').innerHTML =
            '<div class="result-card checkin-success">' +
            '<p style="color:#155724;font-weight:600;margin:0 0 8px">✅ ' + label + '成功！</p>' +
            '<p style="color:#155724;font-size:13px;margin:0">车牌：<strong>' + escapeHtml(plate) + '</strong>' +
            (currentLotName ? ' | 停车场：' + escapeHtml(currentLotName) : '') + '</p>' +
            '</div>';
        document.getElementById('result-actions').style.display = 'none';
        showSuccess('alert-box', plate + ' 入库成功！');
        addHistory(plate, isReserved ? '预约自动入库 ✅' : '确认入库 ✅');
        document.getElementById('manual-plate').value = '';
                const lotRes = await get('/api/parking/list');
        if (lotRes && lotRes.ok) {
            allLots = lotRes.data.lots || [];
            switchToLot(currentLotName);
        }
    } else {
        document.getElementById('checkin-result').innerHTML =
            '<div class="result-card checkin-error"><p style="color:#721c24;font-weight:600;margin:0">❌ 入库失败：' + escapeHtml(res?.data?.error || '未知错误') + '</p></div>';
        showError('alert-box', res?.data?.error || '入库失败');
        addHistory(plate, '入库失败 ❌');
    }
}
function renderRegistrationInfo(plate, reg, recognizeData) {
    const container = document.getElementById('registration-info');
    if (!reg) {
        container.innerHTML = '<p style="color:#999">未查询到登记信息（新车辆）</p>';
        return;
    }
    let html = '<div style="display:flex;flex-wrap:wrap;gap:8px;margin-bottom:8px">';
    if (reg.is_registered) html += '<span class="status-badge green">✅ 已注册</span>';
    else html += '<span class="status-badge yellow">⚠️ 未注册</span>';
    if (reg.in_parking) html += '<span class="status-badge blue">🅿️ 在场</span>';
    if (reg.has_monthly_pass) html += '<span class="status-badge green">🎫 月卡有效</span>';
    if (reg.is_blacklisted) html += '<span class="status-badge red">🚫 黑名单</span>';
    html += '</div>';
    if (recognizeData.confidence && recognizeData.confidence !== '手动输入') {
        const conf = parseFloat(recognizeData.confidence);
        const confColor = conf >= 0.9 ? '#52c41a' : conf >= 0.7 ? '#faad14' : '#ff4d4f';
        html += `<p style="font-size:12px;color:#666;margin:0">置信度: <span style="color:${confColor};font-weight:600">${(conf * 100).toFixed(1)}%</span></p>`;
    }
    if (recognizeData.color && recognizeData.color !== '-') {
        html += `<p style="font-size:12px;color:#666;margin:4px 0 0">车牌颜色: ${escapeHtml(recognizeData.color)}</p>`;
    }
    if (reg.monthly_pass_end) {
        html += `<p style="font-size:12px;color:#666;margin:4px 0 0">月卡到期: ${escapeHtml(reg.monthly_pass_end)}</p>`;
    }
    container.innerHTML = html;
}
function addHistory(plate, result) {
    let history = JSON.parse(localStorage.getItem('user_checkin_history') || '[]');
    history.unshift({
        plate,
        result,
        time: new Date().toLocaleString('zh-CN')
    });
    if (history.length > 20) history = history.slice(0, 20);
    localStorage.setItem('user_checkin_history', JSON.stringify(history));
    renderHistory();
}
function renderHistory() {
    const history = JSON.parse(localStorage.getItem('user_checkin_history') || '[]');
    const container = document.getElementById('history-container');
    if (history.length === 0) {
        container.innerHTML = '<p style="color:#999;font-size:13px">暂无入库记录</p>';
        return;
    }
    container.innerHTML = history.slice(0, 10).map(h => `
        <div style="display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid #f0f0f0;font-size:12px">
            <span><strong>${escapeHtml(h.plate)}</strong> <span style="color:#999;margin-left:6px">${h.result}</span></span>
            <span style="color:#bbb">${h.time}</span>
        </div>
    `).join('');
}
loadLots();
renderHistory();
document.getElementById('manual-plate')?.addEventListener('keydown', e => { if (e.key === 'Enter') manualCheckin(); });
