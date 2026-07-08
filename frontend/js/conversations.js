
const user = checkAuth();
if (user) initSidebar();
setActiveNav('nav-chat');
let st = { currentId: null, filter: 'active', lastId: 0, listTimer: null, replyDirty: false };
function initGate() {
    if (!hasPerm('message.manage')) {
        document.getElementById('cs-admin').style.display = 'none';
        document.getElementById('cs-perm').innerHTML = '<div class="card" style="text-align:center;padding:60px"><h2>权限不足</h2><p style="color:#999;margin-top:8px">您没有管理客服会话的权限</p></div>';
        return false;
    }
    document.getElementById('cs-admin').style.display = 'flex';
    return true;
}
function statusBadge(s) {
    if (s === 'escalated') return '<span class="cs-mini-badge escalated">待处理</span>';
    if (s === 'handled') return '<span class="cs-mini-badge handled">人工接管</span>';
    if (s === 'closed') return '<span class="cs-mini-badge" style="background:#fafafa;color:#999;border:1px solid #d9d9d9">已关闭</span>';
    return '<span class="cs-mini-badge ai">AI 进行中</span>';
}
async function loadList() {
    const res = await get('/api/cs/admin/sessions?status=' + encodeURIComponent(st.filter));
    const box = document.getElementById('cs-list');
    if (!res || !res.ok) { box.innerHTML = '<div class="cs-empty">加载失败</div>'; return; }
    const arr = res.data.sessions || [];
    document.getElementById('cs-count').textContent = arr.length ? (arr.length + ' 个会话') : '';
    if (arr.length === 0) { box.innerHTML = '<div class="cs-empty">暂无会话</div>'; updateBadge(); return; }
    box.innerHTML = arr.map(c => {
        const name = c.truename || c.username || ('用户' + c.user_id);
        const initial = (name.charAt(0) || 'U').toUpperCase();
        const last = (c.last_message || '').substring(0, 26);
        const time = (c.last_time || c.last_message_at || '').split(' ')[1] || '';
        const active = c.id === st.currentId ? ' active' : '';
        const esc2 = c.status === 'escalated' ? ' escalated' : '';
        const unread = c.unread > 0 ? `<span class="cs-unread">${c.unread}</span>` : '';
        return `<div class="cs-conv${active}${esc2}" onclick="selectSession(${c.id})">
            <div class="cs-av ${c.status==='escalated'||c.status==='handled'?'human':'ai'}">${initial}</div>
            <div class="ci">
                <div class="cn"><b>${escapeHtml(name)}</b>${statusBadge(c.status)}${unread}</div>
                <div class="clm">${escapeHtml(last || (c.title || '新会话'))}</div>
                <div class="cm">${time.substring(0,5)}${c.ai_turn_count ? (' · AI '+c.ai_turn_count+'轮') : ''}</div>
            </div>
        </div>`;
    }).join('');
    updateBadge();
}
function setFilter(f) {
    st.filter = f;
    document.querySelectorAll('.cs-filter .f').forEach(el => el.classList.toggle('active', el.dataset.f === f));
    loadList();
}
function fmtContent(text) {
    let s = escapeHtml(text || '');
    s = s.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
    s = s.replace(/\\n/g, '<br>').replace(/\n/g, '<br>');
    return s;
}
function detailBubble(m, uInit) {
    const time = (m.created_at || '').split(' ')[1] || '';
    const t = time ? `<span class="cs-time">${time.substring(0,5)}</span>` : '';
    if (m.sender_type === 'system') return `<div class="cs-sys">${escapeHtml(m.content)}</div>`;
    if (m.sender_type === 'user') {
                return `<div class="cs-row"><div class="cs-av me">${uInit}</div>
            <div class="cs-bubble me">${fmtContent(m.content)}${t}</div></div>`;
    }
    const isAi = m.sender_type === 'assistant';
    const cls = isAi ? 'ai' : 'human';
    const av = isAi ? '&#129302;' : '&#127907;';
        return `<div class="cs-row self"><div class="cs-av ${cls}">${av}</div>
        <div class="cs-bubble ${cls}">${fmtContent(m.content)}${t}</div></div>`;
}
async function selectSession(id) {
    st.currentId = id;
    loadList();
    await renderDetail();
}
async function renderDetail() {
    if (!st.currentId) return;
    const res = await get('/api/cs/admin/session?id=' + st.currentId);
    const box = document.getElementById('cs-detail');
    if (!res || !res.ok || !res.data.found) { box.innerHTML = '<div class="cs-empty">加载失败</div>'; return; }
    const d = res.data;
    const msgs = d.messages || [];
    st.lastId = msgs.length ? msgs[msgs.length - 1].id : 0;
    const name = d.truename || d.username;
    const uInit = (name || 'U').charAt(0).toUpperCase();
    const transcript = msgs.length ? msgs.map(m => detailBubble(m, uInit)).join('') : '<div class="cs-empty">暂无消息</div>';
    const closed = d.status === 'closed';
    box.innerHTML = `
        <div class="mh">
            <div class="cs-av ${d.status==='escalated'||d.status==='handled'?'human':'ai'}">${(name||'U').charAt(0).toUpperCase()}</div>
            <div class="info">
                <b>${escapeHtml(name)} ${statusBadge(d.status)}</b>
                <div class="meta">手机：${escapeHtml(d.telephone||'-')} · 角色：${escapeHtml(d.role||'-')} · 余额：¥${parseFloat(d.balance||0).toFixed(2)} · ${d.ai_turn_count?('AI '+d.ai_turn_count+'轮 · '):''}会话 #${d.id}</div>
            </div>
            ${closed ? '' : '<button class="btn btn-default btn-sm" onclick="closeSession()">关闭会话</button>'}
        </div>
        <div class="cs-admin-transcript" id="cs-transcript">${transcript}</div>
        ${closed ? '<div class="cs-empty">该会话已关闭</div>' : `
        <div class="cs-admin-reply">
            <textarea id="cs-reply-input" rows="1" placeholder="输入回复，回车发送…" maxlength="1000"></textarea>
            <button class="cs-send-btn" onclick="sendReply()">&#10148;</button>
        </div>`}`;
    const ta = document.getElementById('cs-reply-input');
    if (ta) {
        ta.addEventListener('keydown', e => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); sendReply(); } });
        ta.addEventListener('input', () => { ta.style.height = 'auto'; ta.style.height = Math.min(ta.scrollHeight, 100) + 'px'; });
    }
    const tr = document.getElementById('cs-transcript');
    if (tr) tr.scrollTop = tr.scrollHeight;
}
async function sendReply() {
    if (!st.currentId) return;
    const ta = document.getElementById('cs-reply-input');
    const content = (ta.value || '').trim();
    if (!content) return;
    ta.value = '';
    const res = await post('/api/cs/admin/reply', { session_id: st.currentId, content });
    if (res && res.ok) { await renderDetail(); loadList(); }
    else { alert('回复失败：' + (res?.data?.error || '未知错误')); }
}
async function closeSession() {
    if (!st.currentId) return;
    if (!confirm('确定关闭该会话？')) return;
    const res = await post('/api/cs/admin/close', { session_id: st.currentId });
    if (res && res.ok) { await renderDetail(); loadList(); }
}
async function updateBadge() {
    const res = await get('/api/cs/admin/pending');
    const nav = document.getElementById('nav-chat');
    if (!nav) return;
    let badge = nav.querySelector('.nav-badge');
    const n = (res && res.ok) ? (res.data.pending + res.data.unread) : 0;
    if (n > 0) {
        if (!badge) { badge = document.createElement('span'); badge.className = 'nav-badge'; nav.appendChild(badge); }
        badge.textContent = n > 99 ? '99+' : n;
    } else if (badge) {
        badge.remove();
    }
}
function startPolling() {
    if (st.listTimer) clearInterval(st.listTimer);
    st.listTimer = setInterval(async () => {
        loadList();
        if (st.currentId) {
                        const res = await get('/api/cs/admin/session?id=' + st.currentId);
            if (res && res.ok) {
                const msgs = res.data.messages || [];
                const lid = msgs.length ? msgs[msgs.length - 1].id : 0;
                if (lid !== st.lastId) renderDetail();
            }
        }
    }, 4000);
}
if (initGate()) {
    loadList();
    startPolling();
}
