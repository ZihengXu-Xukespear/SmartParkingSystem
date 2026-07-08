
const user = checkAuth();
if (user) initSidebar();
setActiveNav('nav-chat');
const QUICK_QUESTIONS = ['停车怎么收费？', '我的账户余额还有多少？', '现在还有空车位吗？', '怎么办月卡？', '我的车还在停车场吗？'];
let state = {
    sessionId: 0,
    status: 'ai',          
    renderedIds: new Set(),
    busy: false,
    pollTimer: null,
    actionsEl: null,
};
const $msgs = document.getElementById('cs-messages');
const $input = document.getElementById('cs-input');
const $send = document.getElementById('cs-send');
function initial(name) { return ((name || 'U').charAt(0) || 'U').toUpperCase(); }
function scrollBottom() { $msgs.scrollTop = $msgs.scrollHeight; }
function nowTime() {
    const d = new Date();
    return (d.getHours() + '').padStart(2, '0') + ':' + (d.getMinutes() + '').padStart(2, '0');
}
function fmtContent(text) {
    let s = escapeHtml(text || '');
    s = s.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
    s = s.replace(/\\n/g, '<br>').replace(/\n/g, '<br>');
    return s;
}
function updateStatusUI(status) {
    state.status = status;
    const tag = document.getElementById('cs-status-tag');
    const txt = document.getElementById('cs-status-text');
    const name = document.getElementById('cs-agent-name');
    const sub = document.getElementById('cs-agent-sub');
    tag.classList.remove('human');
    if (status === 'ai') {
        txt.textContent = 'AI 客服';
        name.textContent = '小智 · AI 智能客服';
        sub.textContent = '在线 · 平均秒级响应';
    } else if (status === 'escalated') {
        tag.classList.add('human');
        txt.textContent = '等待人工';
        name.textContent = '人工客服';
        sub.textContent = '已为您转接，客服将尽快回复';
    } else if (status === 'handled') {
        tag.classList.add('human');
        txt.textContent = '人工客服在线';
        name.textContent = '人工客服';
        sub.textContent = '客服人员已接入';
    } else {
        txt.textContent = '会话已结束';
        sub.textContent = '如需帮助可重新提问';
    }
        restartPolling();
}
function bubbleHtml(m) {
    const time = (m.created_at || '').split(' ')[1] || nowTime();
    if (m.sender_type === 'system') {
        return `<div class="cs-sys">${escapeHtml(m.content)}</div>`;
    }
    if (m.sender_type === 'user') {
        return `<div class="cs-row self">
            <div class="cs-av me">${initial((user && (user.truename || user.username)))}</div>
            <div class="cs-bubble me">${fmtContent(m.content)}<span class="cs-time">${time.substring(0,5)}</span></div>
        </div>`;
    }
    const isAi = m.sender_type === 'assistant';
    const cls = isAi ? 'ai' : 'human';
    const av = isAi ? '&#129302;' : '&#127907;';
    return `<div class="cs-row">
        <div class="cs-av ${cls}">${av}</div>
        <div class="cs-bubble ${cls}">${fmtContent(m.content)}<span class="cs-time">${time.substring(0,5)}</span></div>
    </div>`;
}
function renderMessage(m) {
    if (state.renderedIds.has(m.id)) return;
    state.renderedIds.add(m.id);
        const w = document.getElementById('cs-welcome');
    if (w) w.remove();
    $msgs.insertAdjacentHTML('beforeend', bubbleHtml(m));
    scrollBottom();
}
function renderWelcome() {
    if (state.renderedIds.size > 0) return;
    if (document.getElementById('cs-welcome')) return;
    const chips = QUICK_QUESTIONS.map(q => `<div class="q" onclick="quickAsk('${q.replace(/'/g, "\\'")}')">${escapeHtml(q)}</div>`).join('');
    $msgs.insertAdjacentHTML('beforeend', `
        <div id="cs-welcome">
            <div class="cs-welcome">
                <div class="big">&#129302;</div>
                <h3>你好，我是小智 👋</h3>
                <p>智慧停车场 AI 智能客服，有任何问题随时问我，也可随时转接人工客服</p>
            </div>
            <div class="cs-row">
                <div class="cs-av ai">&#129302;</div>
                <div class="cs-bubble ai">您好！我是小智，可以帮您查询 <strong>停车费用 / 余额 / 车位 / 月卡</strong> 等，请问有什么可以帮您？</div>
            </div>
            <div class="cs-quick">${chips}</div>
        </div>`);
    scrollBottom();
}
function showActions() {
    hideActions();
    const el = document.createElement('div');
    el.className = 'cs-actions';
    el.innerHTML = `
        <div class="cs-chip-btn continue" onclick="hideActions();document.getElementById('cs-input').focus();">继续询问</div>
        <div class="cs-chip-btn transfer" onclick="doEscalate()">&#128222; 转接人工客服</div>`;
    $msgs.insertAdjacentElement('beforeend', el);
    state.actionsEl = el;
    scrollBottom();
}
function hideActions() {
    if (state.actionsEl) { state.actionsEl.remove(); state.actionsEl = null; }
}
async function loadSession() {
    const res = await get('/api/cs/session');
    if (!res || !res.ok) return;
    state.sessionId = res.data.session_id || 0;
    updateStatusUI(res.data.status || 'ai');
    const msgs = res.data.messages || [];
    if (msgs.length === 0) { renderWelcome(); return; }
    for (const m of msgs) renderMessage(m);
        const last = msgs[msgs.length - 1];
    if (state.status === 'ai' && last && last.sender_type === 'assistant' && state.actionsEl === null && !state.busy) {
            }
}
function restartPolling() {
    if (state.pollTimer) { clearInterval(state.pollTimer); state.pollTimer = null; }
    if (state.status === 'escalated' || state.status === 'handled') {
        state.pollTimer = setInterval(async () => {
            const res = await get('/api/cs/session');
            if (!res || !res.ok) return;
            const msgs = res.data.messages || [];
            for (const m of msgs) renderMessage(m);
        }, 4000);
    }
}
function setBusy(b) {
    state.busy = b;
    $send.disabled = b;
}
function typingHtml() {
    return `<div class="cs-row" id="cs-typing"><div class="cs-av ai">&#129302;</div>
        <div class="cs-bubble ai"><div class="cs-typing"><span></span><span></span><span></span></div></div></div>`;
}
function showTyping() { hideActions(); $msgs.insertAdjacentHTML('beforeend', typingHtml()); scrollBottom(); }
function hideTyping() { const t = document.getElementById('cs-typing'); if (t) t.remove(); }
async function sendMessage() {
    if (state.busy) return;
    const text = ($input.value || '').trim();
    if (!text) return;
    $input.value = '';
    autoGrow();
    hideActions();
        const tempId = 'tmp-' + Date.now();
    const tempMsg = { id: tempId, sender_type: 'user', content: text, created_at: '' };
    state.renderedIds.add(tempId);
    $msgs.insertAdjacentHTML('beforeend', bubbleHtml(tempMsg));
    scrollBottom();
    const wasAi = (state.status === 'ai');
    if (wasAi) { showTyping(); setBusy(true); }
    const res = await post('/api/cs/send', { message: text });
    hideTyping();
    setBusy(false);
    if (!res || !res.ok) {
        $msgs.insertAdjacentHTML('beforeend', `<div class="cs-sys">发送失败：${escapeHtml(res?.data?.error || '网络错误')}，请重试</div>`);
        scrollBottom();
        return;
    }
        state.sessionId = res.data.session_id || state.sessionId;
    if (res.data.mode === 'ai' && res.data.reply) {
                const aiMsg = { id: 'ai-' + Date.now(), sender_type: 'assistant', content: res.data.reply, created_at: '' };
        $msgs.insertAdjacentHTML('beforeend', bubbleHtml(aiMsg));
        state.renderedIds.add(aiMsg.id);
        scrollBottom();
        if (res.data.show_actions) showActions();
    } else if (res.data.mode === 'human') {
                updateStatusUI(res.data.status || 'escalated');
        $msgs.insertAdjacentHTML('beforeend', `<div class="cs-sys">已发送给人工客服，请耐心等待回复…</div>`);
        scrollBottom();
    }
        const sess = await get('/api/cs/session');
    if (sess && sess.ok) {
        for (const m of (sess.data.messages || [])) {
            if (!state.renderedIds.has(m.id)) state.renderedIds.add(m.id);
        }
    }
}
function quickAsk(q) { $input.value = q; sendMessage(); }
async function doEscalate() {
    hideActions();
    const res = await post('/api/cs/escalate', {});
    if (!res || !res.ok) { return; }
    updateStatusUI('escalated');
    const sys = { id: 'sys-' + Date.now(), sender_type: 'system', content: '已为您转接人工客服，客服人员将尽快与您联系，请稍候…', created_at: '' };
    $msgs.insertAdjacentHTML('beforeend', bubbleHtml(sys));
    scrollBottom();
    $input.focus();
}
function autoGrow() {
    $input.style.height = 'auto';
    $input.style.height = Math.min($input.scrollHeight, 120) + 'px';
}
$input.addEventListener('input', autoGrow);
$input.addEventListener('keydown', e => {
    if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); sendMessage(); }
});
loadSession();
autoGrow();
