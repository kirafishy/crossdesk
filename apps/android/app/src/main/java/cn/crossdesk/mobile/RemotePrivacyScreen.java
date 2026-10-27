package cn.crossdesk.mobile;

import org.json.JSONObject;

/** Host-confirmed privacy state; all access belongs to the main thread. */
final class RemotePrivacyScreen {
    boolean desktopAvailable = true;
    boolean received, supported, inputBlockSupported, overlay, inputBlocked, remotePaused, pending;
    boolean unlockNotice;
    long localUnlockRevision;
    int state;
    long revision, requestRevision, requestId, receivedAt, requestedAt;
    String reason = "", failure = "";

    void setInteractiveStage(String stage) {
        desktopAvailable = !"lock-screen".equals(stage) &&
                !"credential-ui".equals(stage) && !"secure-desktop".equals(stage);
    }

    void receive(JSONObject message, long now) {
        try {
            if (message.getInt("type") != 10) return;
            JSONObject value = message.getJSONObject("privacy_status");
            int next = value.getInt("state");
            long rev = value.getLong("revision");
            if (next < 0 || next > 5 || rev < 0 || rev > 0xffffffffL ||
                    received && (int)(rev - revision) < 0) return;
            boolean support = value.getBoolean("supported"), cover = value.getBoolean("overlay_active");
            boolean paused = value.getBoolean("remote_paused");
            if (next == 3 && (!support || !cover || paused)) return;
            boolean blockSupport = value.getBoolean("input_block_supported"), blocked = value.getBoolean("input_blocked");
            String detail = value.getString("reason");
            long unlock = value.has("local_unlock_revision") ? value.getLong("local_unlock_revision") : 0;
            if (unlock < 0 || unlock > 0xffffffffL) return;
            if (received && (int)(unlock - localUnlockRevision) > 0) unlockNotice = true;
            if (next == 3) unlockNotice = false;
            localUnlockRevision = unlock;
            state = next; revision = rev; supported = support; overlay = cover; remotePaused = paused;
            inputBlockSupported = blockSupport; inputBlocked = blocked; reason = detail;
            received = true; receivedAt = now; failure = "";
            if (pending && (int)(revision - requestRevision) > 0 && state != 2 && state != 4) pending = false;
        } catch (org.json.JSONException ignored) { }
    }
    boolean active() { return overlay || remotePaused || state == 2 || state == 3 || state == 4; }
    boolean fresh(long now) { return received && now - receivedAt <= 5000; }
    boolean enabled(long now) { return fresh(now) && state == 3 && overlay && !remotePaused; }
    String buttonTitle(long now) {
        if (enabled(now)) return "隐私屏已开启";
        if (fresh(now) && supported && state == 1 && !active()) return "隐私屏已关闭";
        return "隐私屏";
    }
    String feedback(long now) {
        if (!desktopAvailable) return "";
        if (failure.isEmpty() && (pending || fresh(now) && (state == 2 || state == 4))) return "";
        if (failure.isEmpty() && !pending && fresh(now) && supported &&
                (enabled(now) || state == 1 && !active())) return "";
        String detail = detail(now);
        return status(now) + (detail.isEmpty() ? "" : "：" + detail);
    }
    boolean canToggle(long now) {
        return desktopAvailable && !pending && fresh(now) && state != 4 && (active() || supported && state == 1);
    }
    boolean canLock(long now) {
        return desktopAvailable && !pending && fresh(now) && supported;
    }
    long begin(long now) { return begin(now, false); }
    long begin(long now, boolean lock) {
        tick(now);
        if (!(lock ? canLock(now) : canToggle(now))) return -1;
        pending = true; requestedAt = now; requestRevision = revision; failure = "";
        return ++requestId;
    }
    void failed(long id) {
        if (pending && requestId == id) { pending = false; failure = "指令发送失败，请重试"; }
    }
    void tick(long now) {
        if (pending && now - requestedAt >= 15000) {
            pending = false; failure = "请求超时，尚未确认电脑的隐私屏状态";
        }
    }
    String status(long now) {
        if (!failure.isEmpty()) return failure;
        if (!received) return "等待电脑返回隐私屏状态；旧版电脑端可能不支持";
        if (!fresh(now)) return "隐私屏状态已过期，等待电脑确认";
        if (state == 2) return "正在开启隐私屏…";
        if (state == 4) return "正在关闭隐私屏…";
        if (pending) return "指令已发送，等待电脑确认…";
        if (state == 5) return "隐私屏异常";
        if (!supported || state == 0) return "隐私屏暂不可用";
        if (state == 3 && overlay && !remotePaused)
            return "隐私屏已开启";
        return state == 1 && !active() ? "隐私屏已关闭" : "隐私屏状态待确认";
    }
    String detail(long now) {
        return fresh(now) && (state == 0 || state == 5 || !supported) ? reason : "";
    }
}
