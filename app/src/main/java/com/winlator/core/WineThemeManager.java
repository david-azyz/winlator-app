package com.winlator.core;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;

import com.winlator.win32.MSBitmap;
import com.winlator.xenvironment.RootFS;
import com.winlator.xserver.ScreenInfo;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;

public abstract class WineThemeManager {
    public enum Theme {LIGHT, DARK, MODERN}
    public enum BackgroundType {IMAGE, COLOR}
    public static final String DEFAULT_DESKTOP_THEME = Theme.LIGHT+","+BackgroundType.IMAGE+",#0277bd";
    public static final String DEFAULT_WALLPAPER_ID = "wallpaper-1";
    public static final String MODERN_WALLPAPER_ID = "wallpaper-4";

    public static class ThemeInfo {
        public final Theme theme;
        public final BackgroundType backgroundType;
        public final int backgroundColor;
        public final String wallpaperId;

        public ThemeInfo(String value) {
            String[] values = value.split(",");
            theme = Theme.valueOf(values[0]);

            if (values.length < 3) {
                backgroundColor = Color.parseColor(values[1]);
                backgroundType = BackgroundType.IMAGE;
            }
            else {
                backgroundType = BackgroundType.valueOf(values[1]);
                backgroundColor = Color.parseColor(values[2]);
            }

            String lastValue = values[values.length-1];
            wallpaperId = lastValue.startsWith("wallpaper-") ? lastValue : (lastValue.equals("0") ? DEFAULT_WALLPAPER_ID : "user-wallpaper");
        }
    }

    public static void apply(Context context, ThemeInfo themeInfo, ScreenInfo screenInfo) {
        File rootDir = RootFS.find(context).getRootDir();
        File userRegFile = new File(rootDir, RootFS.WINEPREFIX+"/user.reg");
        String background = Color.red(themeInfo.backgroundColor)+" "+Color.green(themeInfo.backgroundColor)+" "+Color.blue(themeInfo.backgroundColor);

        if (themeInfo.backgroundType == BackgroundType.IMAGE) createWallpaperBMPFile(context, themeInfo.wallpaperId, screenInfo);

        try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
            if (themeInfo.backgroundType == BackgroundType.IMAGE) {
                registryEditor.setStringValue("Control Panel\\Desktop", "Wallpaper", RootFS.getDosUserCachePath()+"\\wallpaper.bmp");
            }
            else registryEditor.removeValue("Control Panel\\Desktop", "Wallpaper");

            if (themeInfo.theme == Theme.LIGHT) {
                registryEditor.setStringValue("Control Panel\\Colors", "ActiveBorder", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "ActiveTitle", "96 125 139");
                registryEditor.setStringValue("Control Panel\\Colors", "Background", background);
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonAlternateFace", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonDkShadow", "158 158 158");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonFace", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonHilight", "224 224 224");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonLight", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonShadow", "158 158 158");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonText", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "GradientActiveTitle", "96 125 139");
                registryEditor.setStringValue("Control Panel\\Colors", "GradientInactiveTitle", "117 117 117");
                registryEditor.setStringValue("Control Panel\\Colors", "GrayText", "158 158 158");
                registryEditor.setStringValue("Control Panel\\Colors", "Hilight", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "HilightText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "HotTrackingColor", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveBorder", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveTitle", "117 117 117");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveTitleText", "200 200 200");
                registryEditor.setStringValue("Control Panel\\Colors", "InfoText", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "InfoWindow", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Menu", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuBar", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuHilight", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuText", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "Scrollbar", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "TitleText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Window", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "WindowFrame", "158 158 158");
                registryEditor.setStringValue("Control Panel\\Colors", "WindowText", "0 0 0");
            }
            else if (themeInfo.theme == Theme.MODERN) {
                applyModernColors(registryEditor, background);
            }
            else if (themeInfo.theme == Theme.DARK) {
                registryEditor.setStringValue("Control Panel\\Colors", "ActiveBorder", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "ActiveTitle", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "Background", background);
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonAlternateFace", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonDkShadow", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonFace", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonHilight", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonLight", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonShadow", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "GradientActiveTitle", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "GradientInactiveTitle", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "GrayText", "117 117 117");
                registryEditor.setStringValue("Control Panel\\Colors", "Hilight", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "HilightText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "HotTrackingColor", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveBorder", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveTitle", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveTitleText", "117 117 117");
                registryEditor.setStringValue("Control Panel\\Colors", "InfoText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "InfoWindow", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Menu", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuBar", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuHilight", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Scrollbar", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "TitleText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Window", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "WindowFrame", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "WindowText", "255 255 255");
            }

            if (themeInfo.theme == Theme.MODERN) {
                applyModernStyle(context, rootDir, registryEditor);
            }
            else restoreClassicStyle(registryEditor);
        }
    }

    private static final String THEME_KEY = "Software\\Microsoft\\Windows\\CurrentVersion\\ThemeManager";
    private static final String THEME_BACKUP_KEY = "Software\\Winlator\\ThemeBackup";
    private static final String[] THEME_VALUES = {"ThemeActive", "DllName", "ColorName", "SizeName"};
    private static final String METRICS_KEY = "Control Panel\\Desktop\\WindowMetrics";
    /** Window metrics set by the Moderno theme, in twips (-15 = 1 pixel). */
    private static final String[][] MODERN_METRICS = {
        {"BorderWidth", "-15"},
        {"PaddedBorderWidth", "0"},
        {"CaptionHeight", "-345"},
        {"CaptionWidth", "-510"},
        {"SmCaptionHeight", "-300"},
        {"SmCaptionWidth", "-300"},
        {"MenuHeight", "-300"},
        {"MenuWidth", "-300"},
        {"ScrollHeight", "-255"},
        {"ScrollWidth", "-255"}
    };

    /** Windows 10 inspired palette: white windows, light gray surfaces, blue accent. */
    private static void applyModernColors(WineRegistryEditor registryEditor, String background) {
        String[][] colors = {
            {"ActiveBorder", "255 255 255"},
            {"ActiveTitle", "255 255 255"},
            {"AppWorkspace", "243 243 243"},
            {"Background", background},
            {"ButtonAlternateFace", "240 240 240"},
            {"ButtonDkShadow", "160 160 160"},
            {"ButtonFace", "240 240 240"},
            {"ButtonHilight", "255 255 255"},
            {"ButtonLight", "245 245 245"},
            {"ButtonShadow", "204 204 204"},
            {"ButtonText", "0 0 0"},
            {"GradientActiveTitle", "255 255 255"},
            {"GradientInactiveTitle", "255 255 255"},
            {"GrayText", "109 109 109"},
            {"Hilight", "0 120 215"},
            {"HilightText", "255 255 255"},
            {"HotTrackingColor", "0 102 204"},
            {"InactiveBorder", "255 255 255"},
            {"InactiveTitle", "255 255 255"},
            {"InactiveTitleText", "153 153 153"},
            {"InfoText", "0 0 0"},
            {"InfoWindow", "255 255 255"},
            {"Menu", "242 242 242"},
            {"MenuBar", "255 255 255"},
            {"MenuHilight", "204 232 255"},
            {"MenuText", "0 0 0"},
            {"Scrollbar", "240 240 240"},
            {"TitleText", "0 0 0"},
            {"Window", "255 255 255"},
            {"WindowFrame", "100 100 100"},
            {"WindowText", "0 0 0"}
        };
        for (String[] color : colors) registryEditor.setStringValue("Control Panel\\Colors", color[0], color[1]);
    }

    /**
     * Activates Wine's built-in "Light" visual style (flat buttons, lists, tabs and
     * scroll bars), thin window borders, flat menus and ClearType text. The previous
     * ThemeManager values are kept in THEME_BACKUP_KEY and restored by the classic themes.
     */
    private static void applyModernStyle(Context context, File rootDir, WineRegistryEditor registryEditor) {
        if (registryEditor.getStringValue(THEME_BACKUP_KEY, "Saved") == null) {
            for (String name : THEME_VALUES) {
                String value = registryEditor.getStringValue(THEME_KEY, name);
                registryEditor.setStringValue(THEME_BACKUP_KEY, name, value != null ? value : "");
            }
            String fontSmoothing = registryEditor.getStringValue("Control Panel\\Desktop", "FontSmoothing");
            registryEditor.setStringValue(THEME_BACKUP_KEY, "FontSmoothing", fontSmoothing != null ? fontSmoothing : "");
            registryEditor.setStringValue(THEME_BACKUP_KEY, "Saved", "1");
        }

        File visualStyle = installLightVisualStyle(rootDir);
        if (visualStyle != null) {
            registryEditor.setStringValue(THEME_KEY, "ThemeActive", "1");
            registryEditor.setStringValue(THEME_KEY, "DllName", "C:\\windows\\resources\\themes\\light\\light.msstyles");
            registryEditor.setStringValue(THEME_KEY, "ColorName", "Blue");
            registryEditor.setStringValue(THEME_KEY, "SizeName", "NormalSize");
        }

        for (String[] metric : MODERN_METRICS) registryEditor.setStringValue(METRICS_KEY, metric[0], metric[1]);

        registryEditor.setHexValues("Control Panel\\Desktop", "UserPreferencesMask", new byte[]{(byte)0x90, 0x32, 0x07, (byte)0x80, 0x10, 0x00, 0x00, 0x00});
        registryEditor.setStringValue("Control Panel\\Desktop", "FontSmoothing", "2");
        registryEditor.setDwordValue("Control Panel\\Desktop", "FontSmoothingType", 2);
        registryEditor.setDwordValue("Control Panel\\Desktop", "FontSmoothingGamma", 1400);
        registryEditor.setDwordValue("Control Panel\\Desktop", "FontSmoothingOrientation", 1);
    }

    private static void restoreClassicStyle(WineRegistryEditor registryEditor) {
        if (registryEditor.getStringValue(THEME_BACKUP_KEY, "Saved") == null) return;

        for (String name : THEME_VALUES) {
            String value = registryEditor.getStringValue(THEME_BACKUP_KEY, name);
            if (value == null || value.isEmpty()) registryEditor.removeValue(THEME_KEY, name);
            else registryEditor.setStringValue(THEME_KEY, name, value);
        }
        String fontSmoothing = registryEditor.getStringValue(THEME_BACKUP_KEY, "FontSmoothing");
        if (fontSmoothing == null || fontSmoothing.isEmpty()) registryEditor.removeValue("Control Panel\\Desktop", "FontSmoothing");
        else registryEditor.setStringValue("Control Panel\\Desktop", "FontSmoothing", fontSmoothing);
        registryEditor.removeKey(THEME_BACKUP_KEY);

        for (String[] metric : MODERN_METRICS) registryEditor.removeValue(METRICS_KEY, metric[0]);
        registryEditor.removeValue("Control Panel\\Desktop", "UserPreferencesMask");
        registryEditor.removeValue("Control Panel\\Desktop", "FontSmoothingType");
        registryEditor.removeValue("Control Panel\\Desktop", "FontSmoothingGamma");
        registryEditor.removeValue("Control Panel\\Desktop", "FontSmoothingOrientation");
    }

    /**
     * Makes sure the container has C:\windows\resources\themes\light\light.msstyles,
     * copying it from the Wine installation in the rootfs when needed.
     * Returns the file, or null when this Wine build has no Light theme.
     */
    private static File installLightVisualStyle(File rootDir) {
        File target = new File(rootDir, RootFS.WINEPREFIX+"/drive_c/windows/resources/themes/light/light.msstyles");
        if (target.isFile() && target.length() > 0) return target;

        File source = findFile(new File(rootDir, "opt"), "light.msstyles", 6);
        if (source == null) return null;

        File parent = target.getParentFile();
        if (parent != null && !parent.isDirectory()) parent.mkdirs();
        return FileUtils.copy(source, target) ? target : null;
    }

    private static File findFile(File dir, String name, int depth) {
        File[] files = dir.listFiles();
        if (files == null || depth < 0) return null;
        File fallback = null;
        for (File file : files) {
            if (file.isFile() && file.getName().equalsIgnoreCase(name)) {
                if (file.getPath().contains("x86_64-windows")) return file;
                if (fallback == null) fallback = file;
            }
        }
        if (fallback != null) return fallback;
        for (File file : files) {
            if (file.isDirectory() && !java.nio.file.Files.isSymbolicLink(file.toPath())) {
                File found = findFile(file, name, depth - 1);
                if (found != null) return found;
            }
        }
        return null;
    }

    private static void createWallpaperBMPFile(Context context, String wallpaperId, ScreenInfo screenInfo) {
        boolean fill = false;
        if (wallpaperId.startsWith("wallpaper-")) {
            try {
                fill = new JSONObject(FileUtils.readString(context, "wallpapers/"+wallpaperId+"/config.json")).optBoolean("fill", false);
            }
            catch (JSONException e) {}
        }
        final int outputHeight = fill ? 720 : 480;
        int outputWidth = (int)Math.ceil(((float)outputHeight / screenInfo.height) * screenInfo.width);

        Bitmap outputBitmap = Bitmap.createBitmap(outputWidth, outputHeight, Bitmap.Config.ARGB_8888);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        Canvas canvas = new Canvas(outputBitmap);

        File userWallpaperFile = getUserWallpaperFile(context);
        if (wallpaperId.equals("user-wallpaper") && userWallpaperFile.isFile()) {
            Bitmap image = BitmapFactory.decodeFile(userWallpaperFile.getPath());
            Rect srcRect = new Rect(0, 0, image.getWidth(), image.getHeight());
            Rect dstRect = new Rect(0, 0, outputWidth, outputHeight);
            canvas.drawBitmap(image, srcRect, dstRect, paint);
        }
        else if (wallpaperId.startsWith("wallpaper-")) {
            String wallpaperDir = "wallpapers/"+wallpaperId;
            JSONObject config;
            try {
                config = new JSONObject(FileUtils.readString(context, wallpaperDir+"/config.json"));
            }
            catch (JSONException e) {
                return;
            }

            int primaryColor = Color.parseColor(config.optString("primaryColor", "#ffffff"));
            int secondaryColor = Color.parseColor(config.optString("secondaryColor", "#ffffff"));
            String orientation = config.optString("orientation", "none");
            float scale = (float)config.optDouble("scale", 1.0);
            Bitmap wallpaperBitmap = ImageUtils.getBitmapFromAsset(context, wallpaperDir+"/image.png");
            paint.setStyle(Paint.Style.FILL);
            paint.setFilterBitmap(true);

            if (config.optBoolean("fill", false)) {
                /* Full-screen picture: scale to cover the screen and crop the center. */
                float imageRatio = (float)wallpaperBitmap.getWidth() / wallpaperBitmap.getHeight();
                float outputRatio = (float)outputWidth / outputHeight;
                Rect srcRect;
                if (imageRatio > outputRatio) {
                    int cropWidth = (int)(wallpaperBitmap.getHeight() * outputRatio);
                    int left = (wallpaperBitmap.getWidth() - cropWidth) / 2;
                    srcRect = new Rect(left, 0, left + cropWidth, wallpaperBitmap.getHeight());
                }
                else {
                    int cropHeight = (int)(wallpaperBitmap.getWidth() / outputRatio);
                    int top = (wallpaperBitmap.getHeight() - cropHeight) / 2;
                    srcRect = new Rect(0, top, wallpaperBitmap.getWidth(), top + cropHeight);
                }
                canvas.drawBitmap(wallpaperBitmap, srcRect, new Rect(0, 0, outputWidth, outputHeight), paint);
                RootFS rootFS = RootFS.find(context);
                MSBitmap.create(outputBitmap, new File(rootFS.getRootDir(), RootFS.USER_CACHE_PATH +"/wallpaper.bmp"));
                return;
            }

            if (orientation.equals("horizontal")) {
                paint.setColor(primaryColor);
                canvas.drawRect(0, 0, outputWidth * 0.5f, outputHeight, paint);
                paint.setColor(secondaryColor);
                canvas.drawRect(outputWidth * 0.5f, 0, outputWidth, outputHeight, paint);
            }
            else if (orientation.equals("vertical")) {
                paint.setColor(primaryColor);
                canvas.drawRect(0, 0, outputWidth, outputHeight * 0.5f, paint);
                paint.setColor(secondaryColor);
                canvas.drawRect(0, outputHeight * 0.5f, outputWidth, outputHeight, paint);
            }

            float targetSize = outputHeight * scale;
            float centerX = (outputWidth - targetSize) * 0.5f;
            float centerY = (outputHeight - targetSize) * 0.5f;
            Rect srcRect = new Rect(0, 0, wallpaperBitmap.getWidth(), wallpaperBitmap.getHeight());
            RectF dstRect = new RectF(centerX, centerY, centerX + targetSize, centerY + targetSize);
            canvas.drawBitmap(wallpaperBitmap, srcRect, dstRect, paint);
        }

        RootFS rootFS = RootFS.find(context);
        MSBitmap.create(outputBitmap, new File(rootFS.getRootDir(), RootFS.USER_CACHE_PATH +"/wallpaper.bmp"));
    }

    public static File getUserWallpaperFile(Context context) {
        return new File(RootFS.find(context).getRootDir(), RootFS.USER_CONFIG_PATH +"/user-wallpaper.png");
    }
}
