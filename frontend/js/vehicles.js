
function showSpotMap(spotNumber) {
    const area = document.getElementById('spot-map-body');
    const cols = 5;
    const zNames = ['A区','B区','C区','D区'];
    const zColors = ['#e3f2fd','#fff8e1','#e8f5e9','#fce4ec'];
    const perZone = 25;
    function rowHtml(zoneStart, rowIdx) {
        let h = '<div style="display:flex;gap:3px;justify-content:center;">';
        for (let c = 0; c < cols; c++) {
            const n = zoneStart + rowIdx * cols + c + 1;
            if (n > 100) { h += '<div style="width:46px;height:46px;"></div>'; continue; }
            const isTarget = n === spotNumber;
            const isEV = (n-1)%25 >= 20;
            let bg = '#d4edda', cl = '#155724', bd = 'transparent', sh = '0 1px 3px rgba(0,0,0,0.06)';
            if (isTarget) { bg = '#ff4d4f'; cl = '#fff'; bd = '#ff4d4f'; sh = '0 0 10px rgba(255,77,79,0.6)'; }
            else if (isEV) { bg = '#cce5ff'; cl = '#004085'; bd = '#2196F3'; }
            h += '<div style="width:46px;height:46px;border-radius:6px;display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:700;border:2px solid '+bd+';background:'+bg+';color:'+cl+';box-shadow:'+sh+';">'+n+'</div>';
        }
        return h + '</div>';
    }
    let html = '<div style="display:inline-block;background:#fafafa;border-radius:8px;padding:12px;">';
        html += '<div style="display:flex;">';
    for (let zi = 0; zi < 2; zi++) {
        html += '<div style="flex:1;background:'+zColors[zi]+';border-radius:4px;padding:4px;border:1px solid #e0e0e0;'+(zi===0?'margin-right:4px;':'margin-left:4px;')+'">';
        html += '<div style="text-align:center;font-size:11px;font-weight:700;color:#555;margin-bottom:2px;">'+zNames[zi]+'</div>';
        for (let r = 0; r < cols; r++) html += rowHtml(zi * perZone, r);
        html += '</div>';
    }
    html += '</div>';
        html += '<div style="display:flex;align-items:center;margin:4px 0;"><div style="flex:1;height:28px;background:#e0e0e0;border-radius:2px;display:flex;align-items:center;justify-content:center;"><span style="font-size:10px;color:#888;">🚗 入口 ➡  ⸺⸺  主干道  ⸺⸺  ➡ 🚗 出口</span></div></div>';
        html += '<div style="display:flex;">';
    for (let zi = 2; zi < 4; zi++) {
        html += '<div style="flex:1;background:'+zColors[zi]+';border-radius:4px;padding:4px;border:1px solid #e0e0e0;'+(zi===2?'margin-right:4px;':'margin-left:4px;')+'">';
        html += '<div style="text-align:center;font-size:11px;font-weight:700;color:#555;margin-bottom:2px;">'+zNames[zi]+'</div>';
        for (let r = 0; r < cols; r++) html += rowHtml(zi * perZone, r);
        html += '</div>';
    }
    html += '</div></div>';
    area.innerHTML = html;
    document.getElementById('spot-map-title').textContent = '📍 车位地图 — 目标: ' + zoneLabel(spotNumber) + ' 号（标红）';
    showModal('spot-map-modal');
}
const user = checkAuth();
if (user) initSidebar();
const canDelete = hasPerm('vehicle.delete');
function zoneLabel(num) {
    if (!num || num < 1) return '-';
    if (num <= 25) return 'A区' + num;
    if (num <= 50) return 'B区' + num;
    if (num <= 75) return 'C区' + num;
    return 'D区' + num;
}
const billingTypes = {
    'standard': '标准计费',
    'tiered': '阶梯计费',
    'member': '会员计费',
    'special': '特殊车辆'
};
async function searchRecords() {
    const plate = document.getElementById('search-plate').value.trim();
    const start = document.getElementById('search-start').value;
    const end = document.getElementById('search-end').value;
    let url = '/api/vehicle/query?';
    if (plate) url += `plate=${encodeURIComponent(plate)}&`;
    if (start) url += `start=${encodeURIComponent(start)}&`;
    if (end) url += `end=${encodeURIComponent(end)}&`;
    const res = await get(url);
    const tbody = document.getElementById('records-table');
    if (!res || !res.ok) {
        tbody.innerHTML = '<tr><td colspan="9" style="text-align:center;color:#999">查询失败</td></tr>';
        return;
    }
    const records = res.data.records || [];
    document.getElementById('record-count').textContent = `共 ${records.length} 条记录`;
    if (records.length === 0) {
        tbody.innerHTML = '<tr><td colspan="10" style="text-align:center;color:#999">暂无记录</td></tr>';
        return;
    }
    tbody.innerHTML = records.map(r => `
        <tr>
            <td>${r.id}</td>
            <td><strong>${r.license_plate}</strong></td>
            <td>${formatDateTime(r.check_in_time)}</td>
            <td>${formatDateTime(r.check_out_time)}</td>
            <td>${formatFee(r.fee)}</td>
            <td>${r.P_name || r.location}</td>
            <td>${r.spot_number ? zoneLabel(r.spot_number) : '-'}</td>
            <td><span class="badge badge-primary">${billingTypes[r.billing_type] || r.billing_type}</span></td>
            <td>${r.spot_number ? '<button class="btn btn-default btn-xs" onclick="showSpotMap('+r.spot_number+')">📍 地图</button>' : '-'}</td>
            <td>
                ${canDelete ? `<button class="btn btn-danger btn-sm" onclick="deleteRecord(${r.id})">删除</button>` : '<span style="color:#999">-</span>'}
            </td>
        </tr>
    `).join('');
}
function resetSearch() {
    document.getElementById('search-plate').value = '';
    document.getElementById('search-start').value = '';
    document.getElementById('search-end').value = '';
    searchRecords();
}
async function deleteRecord(id) {
    if (!confirm('确定要删除这条记录吗？')) return;
    const res = await del('/api/vehicle/' + id);
    if (res && res.ok) {
        showSuccess('search-alert', '删除成功');
        searchRecords();
    } else {
        showError('search-alert', res?.data?.error || '删除失败');
    }
}
document.getElementById('search-plate')?.addEventListener('keydown', e => {
    if (e.key === 'Enter') searchRecords();
});
searchRecords();
