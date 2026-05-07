// Admin page
const user = checkAuth();
if (user) { initSidebar(); if (!hasPerm('user.view')) { document.querySelector('.main-content').innerHTML = '<div class="card" style="text-align:center;padding:60px"><h2>权限不足</h2></div>'; } }

function roleLabel(r) { const m={root:'超级管理员',admin:'管理员',operator:'操作员',user:'普通用户'}; return m[r]||'用户'; }
function roleBadge(r) { const m={root:'badge-danger',admin:'badge-warning',operator:'badge-primary',user:'badge-default'}; return m[r]||'badge-default'; }

function switchTab(tab, ev) {
    document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
    document.querySelectorAll('[id^="tab-"]').forEach(t=>t.classList.add('hidden'));
    if(ev&&ev.target) ev.target.classList.add('active');
    document.getElementById('tab-'+tab).classList.remove('hidden');
    if(tab==='users') loadUsers(); else if(tab==='recharge') loadUsersForRecharge();
    else if(tab==='billing') loadBillingRules(); else if(tab==='passes') loadPasses();
    else if(tab==='plans') loadPlans(); else if(tab==='vehicles') loadVehicles('all');
    else if(tab==='blacklist') { loadBlacklist(); loadInterceptions(); }
    else if(tab==='reports') { loadReports();
        const today = new Date().toISOString().split('T')[0];
        const d30 = new Date(Date.now() - 29*86400000).toISOString().split('T')[0];
        if (!document.getElementById('export-start').value) document.getElementById('export-start').value = d30;
        if (!document.getElementById('export-end').value) document.getElementById('export-end').value = today;
    }
    else if(tab==='notice') loadBulletins();
}

// ========== Users ==========
async function loadUsers() {
    const res = await get('/api/user/list');
    const tbody = document.getElementById('users-table');
    if (!res||!res.ok) { tbody.innerHTML='<tr><td colspan="8">加载失败</td></tr>'; return; }
    const users = res.data.users||[];
    if (!users.length) { tbody.innerHTML='<tr><td colspan="8">暂无用户</td></tr>'; return; }
    tbody.innerHTML = users.map(u=>`<tr data-uid="${u.id}" data-urole="${u.role}">
        <td>${u.id}</td><td><strong>${escapeHtml(u.username)}</strong></td>
        <td>${escapeHtml(u.telephone)}</td><td>${escapeHtml(u.truename)}</td>
        <td><span class="badge ${roleBadge(u.role)}">${roleLabel(u.role)}</span></td>
        <td>¥${parseFloat(u.balance||0).toFixed(2)}</td><td>${formatDateTime(u.created_at)}</td>
        <td><button class="btn btn-default btn-sm btn-edit-user">编辑</button>
        ${u.role==='root'?'<span style="color:#999;font-size:12px">受保护</span>':'<button class="btn btn-danger btn-sm btn-delete-user">删除</button>'}</td></tr>`).join('');
}
function editUser(row) {
    const id = parseInt(row.dataset.uid);
    document.getElementById('user-modal-title').textContent='编辑用户'; document.getElementById('edit-user-id').value=id;
    document.getElementById('modal-username').value=row.cells[1].textContent.trim();
    document.getElementById('modal-password').value=''; document.getElementById('modal-password').placeholder='留空则不修改密码';
    document.getElementById('modal-telephone').value=row.cells[2].textContent.trim();
    document.getElementById('modal-truename').value=row.cells[3].textContent.trim();
    document.getElementById('modal-role').value=row.dataset.urole; showModal('user-modal');
}
async function saveUser() {
    const editId=document.getElementById('edit-user-id').value;
    const username=document.getElementById('modal-username').value.trim(), password=document.getElementById('modal-password').value;
    const telephone=document.getElementById('modal-telephone').value.trim(), truename=document.getElementById('modal-truename').value.trim();
    const role=document.getElementById('modal-role').value;
    if(!username){alert('用户名不能为空');return;}
    let res;
    if(editId) res=await put('/api/user/update',{id:parseInt(editId),username,telephone,truename,role,password:password||undefined});
    else { if(!password){alert('请输入密码');return;} res=await post('/api/user/add',{username,password,telephone,truename,role}); }
    if(res&&res.ok){hideModal('user-modal');showSuccess('alert-box',editId?'用户已更新':'用户已添加');loadUsers();resetUserModal();}
    else alert(res?.data?.error||'操作失败');
}
function resetUserModal() {
    document.getElementById('edit-user-id').value=''; document.getElementById('user-modal-title').textContent='添加用户';
    document.getElementById('modal-username').value=''; document.getElementById('modal-password').value=''; document.getElementById('modal-password').placeholder='请输入密码';
    document.getElementById('modal-telephone').value=''; document.getElementById('modal-truename').value=''; document.getElementById('modal-role').value='user';
}

// ========== Recharge ==========
async function loadUsersForRecharge() {
    const res=await get('/api/user/list'); const sel=document.getElementById('recharge-user');
    if(!res||!res.ok) return;
    sel.innerHTML='<option value="">选择用户...</option>'+res.data.users.map(u=>`<option value="${u.id}">${escapeHtml(u.username)} (${roleLabel(u.role)}) - ¥${parseFloat(u.balance||0).toFixed(2)}</option>`).join('');
}
async function doRecharge() {
    const userId=document.getElementById('recharge-user').value, amount=parseFloat(document.getElementById('recharge-amount').value);
    const desc=document.getElementById('recharge-desc').value.trim()||'管理员充值';
    if(!userId||!amount||amount<=0){showError('recharge-alert','请选择用户并输入有效金额');return;}
    const r=await post('/api/balance/recharge',{user_id:parseInt(userId),amount,description:desc});
    if(r&&r.ok){showSuccess('recharge-alert','充值成功！新余额: ¥'+parseFloat(r.data.balance).toFixed(2));loadUsersForRecharge();}
    else showError('recharge-alert',r?.data?.error||'充值失败');
}

// ========== Pass Plans ==========
async function loadPlans() {
    const res=await get('/api/pass-plans'); const tbody=document.getElementById('plans-table');
    if(!res||!res.ok){tbody.innerHTML='<tr><td colspan="6">加载失败</td></tr>';return;}
    const plans=res.data.plans||[];
    tbody.innerHTML=plans.map(p=>`<tr data-plan-id="${p.id}">
        <td>${escapeHtml(p.plan_name)}</td><td>${p.duration_days}天</td><td>¥${parseFloat(p.price).toFixed(2)}</td>
        <td>${escapeHtml(p.description||'-')}</td><td>${p.is_active?'<span class="badge badge-success">启用</span>':'<span class="badge badge-danger">禁用</span>'}</td>
        <td><button class="btn btn-default btn-sm btn-edit-plan">编辑</button><button class="btn btn-danger btn-sm btn-delete-plan">删除</button></td></tr>`).join('');
}
async function savePlan() {
    const id=document.getElementById('edit-plan-id').value;
    const body={plan_name:document.getElementById('plan-name').value.trim(),duration_days:parseInt(document.getElementById('plan-days').value),price:parseFloat(document.getElementById('plan-price').value),description:document.getElementById('plan-desc').value.trim(),is_active:document.getElementById('plan-active').checked};
    let r; if(id) r=await put('/api/pass-plans/'+id,body); else r=await post('/api/pass-plans',body);
    if(r&&r.ok){hideModal('plan-modal');showSuccess('alert-box','套餐已保存');loadPlans();} else alert(r?.data?.error||'保存失败');
}
function openPlanModal(planId, name, days, price, desc, active) {
    document.getElementById('edit-plan-id').value=planId||''; document.getElementById('plan-name').value=name||'';
    document.getElementById('plan-days').value=days||30; document.getElementById('plan-price').value=price||0;
    document.getElementById('plan-desc').value=desc||''; document.getElementById('plan-active').checked=active!==false; showModal('plan-modal');
}

// ========== Billing Rules ==========
async function loadBillingRules() {
    const res=await get('/api/parking/billing-rules'); const tbody=document.getElementById('billing-table');
    if(!res||!res.ok){tbody.innerHTML='<tr><td colspan="8">加载失败</td></tr>';return;}
    tbody.innerHTML=(res.data.rules||[]).map(r=>`<tr data-billing-id="${r.id}" data-billing-type="${escapeHtml(r.rule_type)}" data-tier-config="${escapeHtml(r.tier_config||'')}">
        <td>${escapeHtml(r.rule_name)}</td><td>${escapeHtml(r.rule_type)}</td><td>${r.free_minutes}</td>
        <td>${parseFloat(r.hourly_rate).toFixed(2)}</td><td>${parseFloat(r.max_daily_fee||0).toFixed(2)}</td>
        <td>${escapeHtml(r.description||'-')}</td><td>${r.is_active?'<span class="badge badge-success">启用</span>':'<span class="badge badge-danger">禁用</span>'}</td>
        <td><button class="btn btn-default btn-sm btn-edit-billing">编辑</button></td></tr>`).join('');
}
function editBillingRule(row) {
    const type=row.dataset.billingType;
    document.getElementById('edit-billing-id').value=parseInt(row.dataset.billingId); document.getElementById('billing-name').value=row.cells[0].textContent.trim();
    document.getElementById('billing-free').value=row.cells[2].textContent.trim(); document.getElementById('billing-rate').value=row.cells[3].textContent.trim();
    document.getElementById('billing-max').value=row.cells[4].textContent.trim()==='-'?'':row.cells[4].textContent.trim();
    document.getElementById('billing-desc').value=row.cells[5].textContent.trim()==='-'?'':row.cells[5].textContent.trim();
    document.getElementById('billing-active').checked=row.cells[6].textContent.includes('启用'); document.getElementById('edit-billing-type').value=type;
    const tg=document.getElementById('tier-config-group'), tc=document.getElementById('billing-tier-config');
    tg.style.display=type==='tiered'?'':'none'; tc.value=row.dataset.tierConfig||''; showModal('billing-modal');
}
async function saveBillingRule() {
    const id=document.getElementById('edit-billing-id').value, type=document.getElementById('edit-billing-type').value;
    const body={rule_name:document.getElementById('billing-name').value.trim(),rule_type:type,free_minutes:parseInt(document.getElementById('billing-free').value),hourly_rate:parseFloat(document.getElementById('billing-rate').value),max_daily_fee:parseFloat(document.getElementById('billing-max').value)||0,description:document.getElementById('billing-desc').value.trim(),is_active:document.getElementById('billing-active').checked};
    if(type==='tiered') body.tier_config=document.getElementById('billing-tier-config').value.trim();
    const r=await put('/api/parking/billing-rules/'+id,body);
    if(r&&r.ok){hideModal('billing-modal');showSuccess('alert-box','规则已更新');loadBillingRules();} else alert(r?.data?.error||'更新失败');
}

// ========== Monthly Passes ==========
async function loadPasses() {
    const res=await get('/api/parking/monthly-passes'); const tbody=document.getElementById('passes-table');
    if(!res||!res.ok){tbody.innerHTML='<tr><td colspan="8">加载失败</td></tr>';return;}
    const passes=res.data.passes||[];
    if(!passes.length){tbody.innerHTML='<tr><td colspan="8">暂无</td></tr>';return;}
    tbody.innerHTML=passes.map(p=>`<tr data-pass-id="${p.id}">
        <td>${p.id}</td><td><strong>${escapeHtml(p.license_plate)}</strong></td><td>${escapeHtml(p.pass_type)}</td>
        <td>${formatDate(p.start_date)}</td><td>${formatDate(p.end_date)}</td><td>¥${parseFloat(p.fee).toFixed(2)}</td>
        <td>${p.is_active?'<span class="badge badge-success">有效</span>':'<span class="badge badge-danger">失效</span>'}</td>
        <td><button class="btn btn-default btn-sm btn-edit-pass">编辑</button><button class="btn btn-danger btn-sm btn-delete-pass">停用</button></td></tr>`).join('');
}
function openPassModal(id) {
    document.getElementById('edit-pass-id').value=id||''; document.getElementById('pass-modal-title').textContent=id?'编辑月卡':'添加月卡';
    if(id){
        const row=document.querySelector(`tr[data-pass-id="${id}"]`);
        if(row){document.getElementById('pass-edit-plate').value=row.cells[1].textContent.trim();document.getElementById('pass-edit-type').value=row.cells[2].textContent.trim();document.getElementById('pass-edit-start').value=row.cells[3].textContent.trim();document.getElementById('pass-edit-end').value=row.cells[4].textContent.trim();document.getElementById('pass-edit-fee').value=parseFloat(row.cells[5].textContent.replace('¥',''));document.getElementById('pass-edit-active').checked=row.cells[6].textContent.includes('有效');}
    } else {
        document.getElementById('pass-edit-plate').value='';document.getElementById('pass-edit-type').value='月卡';document.getElementById('pass-edit-start').value=new Date().toISOString().split('T')[0];const e=new Date();e.setDate(e.getDate()+30);document.getElementById('pass-edit-end').value=e.toISOString().split('T')[0];document.getElementById('pass-edit-fee').value='300';document.getElementById('pass-edit-active').checked=true;
    }
    showModal('pass-modal');
}
async function savePass() {
    const id=document.getElementById('edit-pass-id').value;
    const body={license_plate:document.getElementById('pass-edit-plate').value.trim(),pass_type:document.getElementById('pass-edit-type').value.trim(),start_date:document.getElementById('pass-edit-start').value,end_date:document.getElementById('pass-edit-end').value,fee:parseFloat(document.getElementById('pass-edit-fee').value)};
    let r;
    if(id) r=await put('/api/parking/monthly-passes/'+id,body);
    else r=await post('/api/parking/monthly-passes',body);
    if(r&&r.ok){hideModal('pass-modal');showSuccess('alert-box','月卡已保存');loadPasses();} else alert(r?.data?.error||'保存失败');
}

// ========== Vehicle Management ==========
let allVehicles = [];
let currentFilter = 'all';
async function loadVehicles(filter) {
    currentFilter = filter || 'all';
    const res = await get('/api/vehicle/status');
    const tbody = document.getElementById('vehicles-mgmt-table');
    if (!res || !res.ok) { tbody.innerHTML = '<tr><td colspan="5">加载失败</td></tr>'; return; }
    allVehicles = res.data.vehicles || [];
    renderVehicles(currentFilter);
}
function renderVehicles(filter) {
    currentFilter = filter || currentFilter;
    const tbody = document.getElementById('vehicles-mgmt-table');
    const search = (document.getElementById('vehicle-search')?.value || '').trim().toUpperCase();
    let list = allVehicles;
    if (currentFilter === 'parked') list = allVehicles.filter(v => v.is_parked);
    else if (currentFilter === 'left') list = allVehicles.filter(v => !v.is_parked);
    if (search) list = list.filter(v => v.license_plate.toUpperCase().includes(search));
    if (!list.length) { tbody.innerHTML = '<tr><td colspan="5" style="color:#999">暂无数据</td></tr>'; return; }
    tbody.innerHTML = list.map(v => `<tr>
        <td><strong>${escapeHtml(v.license_plate)}</strong></td>
        <td>${formatDateTime(v.last_check_in)}</td>
        <td>${v.is_parked ? '<span class="badge badge-primary">停放中</span>' : '<span class="badge badge-success">已离开</span>'}</td>
        <td>${v.total_records}</td>
        <td>${v.is_parked ? '<button class="btn btn-danger btn-sm btn-checkout-vehicle">出库</button>' : '<span style="color:#999">-</span>'}</td>
    </tr>`).join('');
}
async function checkoutVehicle(plate) {
    if (!confirm('确定对 ' + plate + ' 执行出库操作吗？费用将从您的余额扣除。')) return;
    const res = await post('/api/vehicle/checkout', { license_plate: plate });
    if (res && res.ok) { showSuccess('vehicle-mgmt-alert', plate + ' 出库成功！费用: ¥' + parseFloat(res.data.fee).toFixed(2) + '。请在10分钟内驶离'); loadVehicles('all'); }
    else showError('vehicle-mgmt-alert', res?.data?.error || '出库失败');
}

// ========== Blacklist ==========
async function loadBlacklist() {
    const res=await get('/api/blacklist'); const tbody=document.getElementById('blacklist-table');
    if(!res||!res.ok){tbody.innerHTML='<tr><td colspan="5">加载失败</td></tr>';return;}
    const list=res.data.blacklist||[];
    if(!list.length){tbody.innerHTML='<tr><td colspan="5">暂无</td></tr>';return;}
    tbody.innerHTML=list.map(e=>`<tr data-bl-id="${e.id}"><td>${e.id}</td><td><strong>${escapeHtml(e.license_plate)}</strong></td><td>${escapeHtml(e.reason||'-')}</td><td>${formatDateTime(e.created_at)}</td><td><button class="btn btn-danger btn-sm btn-remove-blacklist">移除</button></td></tr>`).join('');
}
async function addBlacklist() {
    const plate=document.getElementById('bl-plate').value.trim(), reason=document.getElementById('bl-reason').value.trim();
    if(!plate){showError('bl-alert','请输入车牌号');return;}
    const r=await post('/api/blacklist',{license_plate:plate,reason});
    if(r&&r.ok){showSuccess('bl-alert','已添加');document.getElementById('bl-plate').value='';document.getElementById('bl-reason').value='';loadBlacklist();}
    else showError('bl-alert',r?.data?.error||'添加失败');
}

// ========== Reports ==========
async function loadReports() {
    const summary=await get('/api/report/summary');
    if(summary&&summary.ok){const s=summary.data;document.getElementById('rpt-today').textContent='¥'+parseFloat(s.today_income).toFixed(2);document.getElementById('rpt-month').textContent='¥'+parseFloat(s.month_income).toFixed(2);document.getElementById('rpt-total').textContent='¥'+parseFloat(s.total_income).toFixed(2);document.getElementById('rpt-parking').textContent='¥'+parseFloat(s.parking_fees).toFixed(2);document.getElementById('rpt-passes').textContent='¥'+parseFloat(s.pass_sales).toFixed(2);document.getElementById('rpt-reserve').textContent='¥'+parseFloat(s.reservation_prepaid).toFixed(2);}
    const daily=await get('/api/report/daily');
    if(daily&&daily.ok){const dom=document.getElementById('revenue-chart');if(!dom)return;const chart=echarts.init(dom);chart.setOption({tooltip:{trigger:'axis'},xAxis:{type:'category',data:daily.data.dates,axisLabel:{rotate:45,fontSize:10}},yAxis:{type:'value'},series:[{type:'line',data:daily.data.values,smooth:true,itemStyle:{color:'#1890ff'},areaStyle:{color:'rgba(24,144,255,0.1)'}}],grid:{left:40,right:20,top:20,bottom:50}});}
}

// ========== Event delegation ==========
document.addEventListener('click',function(e){
    const t=e.target; if(!t||!t.classList) return;
    const row=t.closest('tr'); if(!row) return;
    if(t.classList.contains('btn-edit-user')) { editUser(row); }
    else if(t.classList.contains('btn-delete-user')) { deleteUser(parseInt(row.dataset.uid)); }
    else if(t.classList.contains('btn-edit-plan')) {
        const pid=parseInt(row.dataset.planId), cells=row.cells;
        openPlanModal(pid,cells[0].textContent.trim(),parseInt(cells[1].textContent),parseFloat(cells[2].textContent.replace('¥','')),cells[3].textContent.trim(),cells[4].textContent.includes('启用'));
    }
    else if(t.classList.contains('btn-delete-plan')) { deletePlan(parseInt(row.dataset.planId)); }
    else if(t.classList.contains('btn-edit-billing')) { editBillingRule(row); }
    else if(t.classList.contains('btn-edit-pass')) { openPassModal(parseInt(row.dataset.passId)); }
    else if(t.classList.contains('btn-delete-pass')) { deactivatePass(parseInt(row.dataset.passId)); }
    else if(t.classList.contains('btn-remove-blacklist')) { removeBlacklist(parseInt(row.dataset.blId)); }
    else if(t.classList.contains('btn-checkout-vehicle')) { checkoutVehicle(row.cells[0].textContent.trim()); }
    else if(t.classList.contains('btn-edit-bulletin')) { openBulletinModal(parseInt(row.dataset.bulletinId)); }
    else if(t.classList.contains('btn-delete-bulletin')) { deleteBulletin(parseInt(row.dataset.bulletinId)); }
});

async function deleteUser(id) { if(!confirm('确定删除用户(ID:'+id+')？'))return; const r=await del('/api/user/'+id); if(r&&r.ok){showSuccess('alert-box','已删除');loadUsers();} else showError('alert-box',r?.data?.error||'删除失败'); }
async function deletePlan(id) { if(!confirm('确定删除此套餐？'))return; const r=await del('/api/pass-plans/'+id); if(r&&r.ok){showSuccess('alert-box','已删除');loadPlans();} else showError('alert-box','删除失败'); }
async function deactivatePass(id) { if(!confirm('确定停用此月卡？停用后该车牌恢复收费。'))return; const r=await del('/api/parking/monthly-passes/'+id); if(r&&r.ok){showSuccess('alert-box','月卡已停用');loadPasses();} else showError('alert-box','停用失败'); }
async function removeBlacklist(id) { if(!confirm('确定移除？'))return; const r=await del('/api/blacklist/'+id); if(r&&r.ok){showSuccess('alert-box','已移除');loadBlacklist();} }

async function loadInterceptions() {
    const res = await get('/api/blacklist/interceptions');
    const tbody = document.getElementById('interception-table');
    if (!res || !res.ok) { tbody.innerHTML = '<tr><td colspan="4">加载失败</td></tr>'; return; }
    const list = res.data.interceptions || [];
    if (!list.length) { tbody.innerHTML = '<tr><td colspan="4" style="color:#999">暂无拦截记录</td></tr>'; return; }
    tbody.innerHTML = list.map(l => '<tr><td>' + l.id + '</td><td><strong>' + escapeHtml(l.license_plate) + '</strong></td><td>' + escapeHtml(l.reason || '-') + '</td><td>' + formatDateTime(l.intercepted_at) + '</td></tr>').join('');
}

async function exportReport() {
    const start = document.getElementById('export-start').value || '';
    const end = document.getElementById('export-end').value || '';
    const token = localStorage.getItem('token');
    const params = new URLSearchParams();
    if (start) params.set('start', start);
    if (end) params.set('end', end);
    const url = '/api/report/export' + (params.toString() ? '?' + params.toString() : '');
    const resp = await fetch(url, { headers: { 'Authorization': 'Bearer ' + token } });
    if (!resp.ok) { showError('export-alert', '导出失败'); return; }
    const blob = await resp.blob();
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'report' + (start ? '_' + start : '') + (end ? '_' + end : '') + '.csv';
    document.body.appendChild(a); a.click(); document.body.removeChild(a);
    URL.revokeObjectURL(a.href);
}

// ========== Bulletin ==========
async function loadBulletins() {
    const res = await get('/api/bulletin/all');
    const tbody = document.getElementById('bulletin-table');
    if (!res || !res.ok) { tbody.innerHTML = '<tr><td colspan="6">加载失败</td></tr>'; return; }
    const list = res.data.bulletins || [];
    if (!list.length) { tbody.innerHTML = '<tr><td colspan="6" style="color:#999">暂无公告</td></tr>'; return; }
    tbody.innerHTML = list.map(b => `<tr data-bulletin-id="${b.id}">
        <td>${b.id}</td>
        <td>${escapeHtml((b.content||'').substring(0, 50))}${(b.content||'').length > 50 ? '...' : ''}</td>
        <td>${b.is_pinned ? '<span class="badge badge-danger">置顶</span>' : '<span class="badge badge-default">否</span>'}</td>
        <td>${b.valid_from ? formatDateTime(b.valid_from) : '-'}</td>
        <td>${b.valid_until ? formatDateTime(b.valid_until) : '-'}</td>
        <td><button class="btn btn-default btn-sm btn-edit-bulletin">编辑</button> <button class="btn btn-danger btn-sm btn-delete-bulletin">删除</button></td>
    </tr>`).join('');
}
function openBulletinModal(id) {
    document.getElementById('edit-bulletin-id').value = id || '';
    document.getElementById('bulletin-modal-title').textContent = id ? '编辑公告' : '新建公告';
    if (id) {
        const row = document.querySelector(`tr[data-bulletin-id="${id}"]`);
        if (row) {
            document.getElementById('bulletin-content-input').value = '';
            document.getElementById('bulletin-valid-from').value = '';
            document.getElementById('bulletin-valid-until').value = '';
            document.getElementById('bulletin-is-pinned').checked = row.cells[2].textContent.includes('置顶');
        }
        // Fetch full content for editing
        get('/api/bulletin/all').then(res => {
            if (res && res.ok) {
                const b = (res.data.bulletins || []).find(x => x.id === id);
                if (b) {
                    document.getElementById('bulletin-content-input').value = b.content || '';
                    document.getElementById('bulletin-valid-from').value = b.valid_from ? b.valid_from.replace(' ', 'T').substring(0, 16) : '';
                    document.getElementById('bulletin-valid-until').value = b.valid_until ? b.valid_until.replace(' ', 'T').substring(0, 16) : '';
                    document.getElementById('bulletin-is-pinned').checked = !!b.is_pinned;
                }
            }
        });
    } else {
        document.getElementById('bulletin-content-input').value = '';
        document.getElementById('bulletin-valid-from').value = '';
        document.getElementById('bulletin-valid-until').value = '';
        document.getElementById('bulletin-is-pinned').checked = false;
    }
    showModal('bulletin-modal');
}
async function saveBulletin() {
    const id = document.getElementById('edit-bulletin-id').value;
    const content = document.getElementById('bulletin-content-input').value.trim();
    const is_pinned = document.getElementById('bulletin-is-pinned').checked;
    const valid_from = document.getElementById('bulletin-valid-from').value;
    const valid_until = document.getElementById('bulletin-valid-until').value;
    if (!content) { showError('notice-alert', '内容不能为空'); return; }
    const body = { content, is_pinned, valid_from: valid_from || '', valid_until: valid_until || '' };
    let r;
    if (id) r = await put('/api/bulletin/' + id, body);
    else r = await post('/api/bulletin', body);
    if (r && r.ok) { hideModal('bulletin-modal'); showSuccess('notice-alert', id ? '公告已更新' : '公告已创建'); loadBulletins(); }
    else showError('notice-alert', r?.data?.error || '保存失败');
}
async function deleteBulletin(id) {
    if (!confirm('确定删除此公告？')) return;
    const r = await del('/api/bulletin/' + id);
    if (r && r.ok) { showSuccess('notice-alert', '公告已删除'); loadBulletins(); }
    else showError('notice-alert', r?.data?.error || '删除失败');
}

loadUsers();
