const user = checkAuth();
if (user) { initSidebar(); if (!hasPerm('parking.settings')) { document.querySelector('.main-content').innerHTML = '<div class="card" style="text-align:center;padding:60px"><h2>权限不足</h2></div>'; } }

async function loadLots() {
    const container = document.getElementById('lots-container');
    const res = await get('/api/parking/list');
    if (!res || !res.ok || !res.data.lots) { container.innerHTML = '<p style="color:#999">加载失败</p>'; return; }
    const lots = res.data.lots;
    if (!lots.length) { container.innerHTML = '<p style="color:#999">暂无停车场</p>'; return; }
    container.innerHTML = lots.map(l => `
        <div class="lot-row" data-pid="${l.P_id}" data-pname="${escapeHtml(l.P_name)}" style="display:flex;align-items:center;gap:10px;padding:10px 0;border-bottom:1px solid #f0f0f0;flex-wrap:wrap;">
            <input type="text" class="form-control" value="${escapeHtml(l.P_name)}" style="width:90px;" data-field="name">
            <input type="number" class="form-control" value="${l.P_total_count}" style="width:65px;" data-field="cap">
            <span style="color:#999;font-size:12px;">车位</span>
            <input type="number" class="form-control" value="${l.P_fee}" style="width:65px;" step="0.5" data-field="fee">
            <span style="color:#999;font-size:12px;">元/时</span>
            <button class="btn btn-primary btn-sm save-lot-btn">保存</button>
            <span style="font-size:12px;color:#666;">已用:${l.P_current_count} 预约:${l.P_reserve_count} 剩余:${l.P_total_count - l.P_current_count - l.P_reserve_count}</span>
            <button class="btn btn-danger btn-sm delete-lot-btn" style="margin-left:auto;">删除</button>
        </div>`).join('');

    // Event delegation
    container.querySelectorAll('.save-lot-btn').forEach(btn => {
        btn.onclick = function() {
            const row = this.closest('.lot-row');
            const pid = parseInt(row.dataset.pid);
            const pname = row.dataset.pname;
            const newName = row.querySelector('[data-field="name"]').value.trim();
            const cap = parseInt(row.querySelector('[data-field="cap"]').value);
            const fee = parseFloat(row.querySelector('[data-field="fee"]').value);
            saveLot(pid, pname, newName, cap, fee);
        };
    });
    container.querySelectorAll('.delete-lot-btn').forEach(btn => {
        btn.onclick = function() {
            const row = this.closest('.lot-row');
            deleteLot(parseInt(row.dataset.pid), row.dataset.pname);
        };
    });
}

async function saveLot(pid, pname, newName, capacity, fee) {
    const body = { P_name: pname, fee, capacity };
    if (newName && newName !== pname) body.new_name = newName;
    const res = await put('/api/parking/settings', body);
    if (res && res.ok) { showSuccess('alert-box', (newName || pname) + ' 已更新'); loadLots(); }
    else showError('alert-box', res?.data?.error || '更新失败');
}

async function addLot() {
    const name = document.getElementById('new-lot-name').value.trim();
    const capacity = parseInt(document.getElementById('new-lot-capacity').value);
    const fee = parseFloat(document.getElementById('new-lot-fee').value);
    if (!name) { showError('alert-box', '请输入名称'); return; }
    const res = await post('/api/parking/lot', { P_name: name, capacity, fee });
    if (res && res.ok) { showSuccess('alert-box', '已添加'); document.getElementById('new-lot-name').value = ''; loadLots(); }
    else showError('alert-box', res?.data?.error || '添加失败');
}

async function deleteLot(id, name) {
    if (!confirm('确定删除「' + name + '」？此操作不可撤销。')) return;
    const res = await request('/api/parking/lot/' + id, { method: 'DELETE' });
    if (res && res.ok) { showSuccess('alert-box', '已删除'); loadLots(); }
    else showError('alert-box', res?.data?.error || '删除失败');
}

loadLots();
