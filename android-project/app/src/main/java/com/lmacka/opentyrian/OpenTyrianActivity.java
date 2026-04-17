package com.lmacka.opentyrian;

import android.content.res.AssetManager;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.libsdl.app.SDLActivity;

public class OpenTyrianActivity extends SDLActivity {
    private static final String TAG = "OpenTyrian";
    private static final String ASSET_ROOT = "tyrian";
    private static final String SENTINEL_FILE = "tyrian1.lvl";

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "SDL2_net",
            "main"
        };
    }

    @Override
    protected String[] getArguments() {
        File dataDir = new File(getFilesDir(), ASSET_ROOT);
        if (!new File(dataDir, SENTINEL_FILE).exists()) {
            try {
                extractAssetTree(ASSET_ROOT, dataDir);
            } catch (IOException e) {
                Log.e(TAG, "Failed to extract game data from assets", e);
            }
        }
        return new String[] { "--data", dataDir.getAbsolutePath() };
    }

    private void extractAssetTree(String assetPath, File destDir) throws IOException {
        AssetManager am = getAssets();
        String[] entries = am.list(assetPath);
        if (entries == null || entries.length == 0) {
            return;
        }
        if (!destDir.exists() && !destDir.mkdirs()) {
            throw new IOException("Cannot create " + destDir);
        }
        byte[] buffer = new byte[16 * 1024];
        for (String entry : entries) {
            String childAssetPath = assetPath + "/" + entry;
            String[] childEntries = am.list(childAssetPath);
            if (childEntries != null && childEntries.length > 0) {
                extractAssetTree(childAssetPath, new File(destDir, entry));
                continue;
            }
            File outFile = new File(destDir, entry);
            try (InputStream in = am.open(childAssetPath);
                 OutputStream out = new FileOutputStream(outFile)) {
                int n;
                while ((n = in.read(buffer)) > 0) {
                    out.write(buffer, 0, n);
                }
            }
        }
    }
}
