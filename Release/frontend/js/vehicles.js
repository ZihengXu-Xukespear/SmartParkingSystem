// 车辆信息页面逻辑

const user = checkAuth();
if (user) initSidebar();
// Only admin can access vehicle records page
if (!hasPerm('vehicle.query')) {
    document.querySelector('.main-content').innerHTML = '<div class="card" style="text-align:center;padding:60px"><h2>权限不足</h2><p style="color:#999;margin-top:12px">此页面需要管理员权限</p><a href="/dashboard.html" class="btn btn-primary mt-3">返回主面板</a></div>';
}
const canDelete = hasPerm('vehicle.delete');

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
        tbody.innerHTML = '<tr><td colspan="9" style="text-align:center;color:#999">暂无记录</td></tr>';
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
            <td>${r.spot_number ? r.spot_number+'号' : '-'}</td>
            <td><span class="badge badge-primary">${billingTypes[r.billing_type] || r.billing_type}</span></td>
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

// Enter key support
document.getElementById('search-plate')?.addEventListener('keydown', e => {
    if (e.key === 'Enter') searchRecords();
});

// Load all records on page load
searchRecords();
