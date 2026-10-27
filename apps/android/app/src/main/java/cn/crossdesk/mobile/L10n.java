package cn.crossdesk.mobile;

import android.content.Context;
import java.util.Locale;

/** Native resource lookup; never translates remote content, file names or protocol data. */
final class L10n {
    private static volatile Context application;
    static void initialize(Context context) { application = context.getApplicationContext(); }
    static String tr(String key) {
        Context context = application;
        Integer id = LocalizationKeys.IDS.get(key);
        return context == null || id == null ? key : context.getString(id);
    }
    static String format(String key, Object... args) {
        java.util.regex.Matcher matcher = java.util.regex.Pattern.compile("\\{(\\d+)\\}").matcher(tr(key));
        StringBuffer result = new StringBuffer();
        while (matcher.find()) {
            int index = Integer.parseInt(matcher.group(1));
            String value = index < args.length ? String.valueOf(args[index]) : matcher.group();
            matcher.appendReplacement(result, java.util.regex.Matcher.quoteReplacement(value));
        }
        matcher.appendTail(result);
        return result.toString();
    }
    static boolean isChinese() {
        Context context = application;
        Locale locale = context == null ? Locale.getDefault()
                : context.getResources().getConfiguration().getLocales().get(0);
        return locale.getLanguage().equals("zh");
    }
    private L10n() {}
}
