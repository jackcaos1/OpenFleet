package dev.openfleet.client;

import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.UriPermission;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.content.ClipData;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Duenne Huelle um die NativeActivity: Ordnerauswahl ueber das Storage Access Framework und Dateizugriff auf den
 * gewaehlten Spielordner. Der gesamte Spielcode liegt in C++; die nativen Methoden unten ruft nur der C++-Teil auf.
 * Es werden keine Spieldateien kopiert, gelesen wird direkt ueber Dateideskriptoren.
 */
public class MainActivity extends NativeActivity {
    private static final int REQ_TREE = 1001;
    private static final String PREFS = "fleetclient";
    private static final int REQ_ZIPS = 1002;
    private static final String KEY_TREE = "tree_uri";
    private static final String KEY_ZIPS = "zip_uris";  // Zeilen, Reihenfolge = Rangfolge (oben gewinnt)

    private static final class Child {
        final String docId;
        final String name;
        final boolean dir;

        Child(String docId, String name, boolean dir) {
            this.docId = docId;
            this.name = name;
            this.dir = dir;
        }
    }

    private Uri treeUri;
    private final List<Uri> zips = new ArrayList<>();
    private volatile int generation = 0;
    // docId eines Ordners -> kleingeschriebener Name -> Kind
    private final Map<String, Map<String, Child>> listings = new HashMap<>();

    /**
     * Echter Vollbildmodus: Navigations- und Statusleiste werden ausgeblendet (Wischen vom Rand zeigt sie kurz an). Ohne das
     * schiebt sich die Spielflaeche nur unter die Leisten, und am Rand liegende Bedienelemente waeren nicht erreichbar.
     */
    private void hideSystemBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.systemBars());
                c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemBars();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemBars();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemBars();
        String saved = getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_TREE, null);
        if (saved != null) {
            Uri u = Uri.parse(saved);
            if (hasPersistedPermission(u)) {
                treeUri = u;
                generation++;
            }
        }
        String z = getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_ZIPS, "");
        for (String line : z.split("\n")) {
            if (line.isEmpty()) {
                continue;
            }
            Uri u = Uri.parse(line);
            if (hasPersistedPermission(u)) {
                zips.add(u);
            }
        }
        if (!zips.isEmpty()) {
            generation++;
        }
        if (treeUri == null && zips.isEmpty()) {
            pickFolder();
        }
    }

    private void saveZips() {
        StringBuilder b = new StringBuilder();
        for (Uri u : zips) {
            b.append(u.toString()).append('\n');
        }
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString(KEY_ZIPS, b.toString()).apply();
        generation++;
    }

    private String displayName(Uri u) {
        try (Cursor c = getContentResolver().query(u, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (c != null && c.moveToFirst() && c.getString(0) != null) {
                return c.getString(0);
            }
        } catch (Exception e) {
            // Name nicht lesbar
        }
        return u.getLastPathSegment() == null ? u.toString() : u.getLastPathSegment();
    }

    /** Dialog "Spieldaten": Ordner waehlen, ZIP-Dateien hinzufuegen, Rangfolge aendern. Oben = hoechste Prioritaet. */
    private void showSourceDialog() {
        final List<String> labels = new ArrayList<>();
        labels.add("Ordner w\u00e4hlen \u2026" + (treeUri != null ? "  (gew\u00e4hlt, niedrigste Priorit\u00e4t)" : ""));
        labels.add("ZIP-Dateien hinzuf\u00fcgen \u2026");
        int first = labels.size();
        for (int i = 0; i < zips.size(); i++) {
            labels.add((i + 1) + ". " + displayName(zips.get(i)));
        }
        if (treeUri != null) {
            labels.add("Ordner entfernen");
        }
        final int folderRemoveIndex = treeUri != null ? labels.size() - 1 : -1;
        final int firstZip = first;
        new AlertDialog.Builder(this)
                .setTitle("Spieldaten (ZIP oben = zuerst)")
                .setItems(labels.toArray(new String[0]), (d, which) -> {
                    if (which == 0) {
                        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                        startActivityForResult(i, REQ_TREE);
                    } else if (which == 1) {
                        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                        i.addCategory(Intent.CATEGORY_OPENABLE);
                        i.setType("*/*");
                        i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {"application/zip", "application/x-zip-compressed", "application/octet-stream"});
                        i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                        startActivityForResult(i, REQ_ZIPS);
                    } else if (which == folderRemoveIndex) {
                        synchronized (MainActivity.this) {
                            treeUri = null;
                            listings.clear();
                        }
                        getSharedPreferences(PREFS, MODE_PRIVATE).edit().remove(KEY_TREE).apply();
                        generation++;
                        showSourceDialog();
                    } else {
                        showZipDialog(which - firstZip);
                    }
                })
                .setNegativeButton("Schlie\u00dfen", null)
                .show();
    }

    private void showZipDialog(final int index) {
        if (index < 0 || index >= zips.size()) {
            showSourceDialog();
            return;
        }
        final String[] items = {"Nach oben (h\u00f6here Priorit\u00e4t)", "Nach unten", "Entfernen"};
        new AlertDialog.Builder(this)
                .setTitle(displayName(zips.get(index)))
                .setItems(items, (d, which) -> {
                    synchronized (MainActivity.this) {
                        if (which == 0 && index > 0) {
                            zips.add(index - 1, zips.remove(index));
                        } else if (which == 1 && index < zips.size() - 1) {
                            zips.add(index + 1, zips.remove(index));
                        } else if (which == 2) {
                            zips.remove(index);
                        }
                    }
                    saveZips();
                    showSourceDialog();
                })
                .setNegativeButton("Zur\u00fcck", (d, w) -> showSourceDialog())
                .show();
    }

    private boolean hasPersistedPermission(Uri u) {
        for (UriPermission p : getContentResolver().getPersistedUriPermissions()) {
            if (p.getUri().equals(u) && p.isReadPermission()) {
                return true;
            }
        }
        return false;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_ZIPS) {
            if (resultCode == RESULT_OK && data != null) {
                List<Uri> picked = new ArrayList<>();
                ClipData clip = data.getClipData();
                if (clip != null) {
                    for (int i = 0; i < clip.getItemCount(); i++) {
                        picked.add(clip.getItemAt(i).getUri());
                    }
                } else if (data.getData() != null) {
                    picked.add(data.getData());
                }
                synchronized (this) {
                    for (Uri u : picked) {
                        try {
                            getContentResolver().takePersistableUriPermission(u, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                        } catch (Exception e) {
                            // nicht dauerhaft freigebbar: gilt nur bis zum Neustart
                        }
                        if (!zips.contains(u)) {
                            zips.add(u);
                        }
                    }
                }
                saveZips();
            }
            showSourceDialog();
            return;
        }
        if (requestCode != REQ_TREE) {
            return;
        }
        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            showSourceDialog();
            return;
        }
        Uri u = data.getData();
        getContentResolver().takePersistableUriPermission(u, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        getSharedPreferences(PREFS, MODE_PRIVATE).edit().putString(KEY_TREE, u.toString()).apply();
        synchronized (this) {
            treeUri = u;
            listings.clear();
        }
        generation++;
        showSourceDialog();
    }

    // ---- Aufrufe aus C++ ----------------------------------------------------------------------------------------

    public int dataGeneration() {
        return generation;
    }

    public boolean hasFolder() {
        return treeUri != null;
    }

    public synchronized int zipCount() {
        return zips.size();
    }

    /** Dateideskriptor der i-ten ZIP (Besitz geht an den Aufrufer) oder -1. */
    public synchronized int openZipFd(int i) {
        try {
            if (i < 0 || i >= zips.size()) {
                return -1;
            }
            ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(zips.get(i), "r");
            return pfd == null ? -1 : pfd.detachFd();
        } catch (Exception e) {
            return -1;
        }
    }

    /** Oeffnet den Dialog fuer Ordner, ZIP-Dateien und deren Rangfolge. */
    public void pickFolder() {
        runOnUiThread(this::showSourceDialog);
    }

    public void showMessage(final String text) {
        // Die Anzeige uebernimmt die Textebene im Spiel; hier nur noch ins Log.
        android.util.Log.i("fleetclient", text);
    }

    /** Namen der Eintraege eines Ordners (relativ zur Spielwurzel, '/'-getrennt, Schreibweise egal). */
    public synchronized String[] listDir(String rel) {
        String docId = resolve(rel);
        if (docId == null) {
            return new String[0];
        }
        Map<String, Child> kids = children(docId);
        List<String> names = new ArrayList<>();
        for (Child c : kids.values()) {
            names.add(c.name);
        }
        return names.toArray(new String[0]);
    }

    public synchronized boolean exists(String rel) {
        return resolve(rel) != null;
    }

    /** Dateideskriptor zum Lesen (Besitz geht an den Aufrufer, der ihn schliesst) oder -1. */
    public synchronized int openFd(String rel) {
        try {
            String docId = resolve(rel);
            if (docId == null || treeUri == null) {
                return -1;
            }
            Uri doc = DocumentsContract.buildDocumentUriUsingTree(treeUri, docId);
            ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(doc, "r");
            return pfd == null ? -1 : pfd.detachFd();
        } catch (Exception e) {
            return -1;
        }
    }

    // ---- intern -----------------------------------------------------------------------------------------------

    private String resolve(String rel) {
        if (treeUri == null) {
            return null;
        }
        String docId = DocumentsContract.getTreeDocumentId(treeUri);
        for (String part : rel.split("/")) {
            if (part.isEmpty() || part.equals(".")) {
                continue;
            }
            Child c = children(docId).get(part.toLowerCase(Locale.ROOT));
            if (c == null) {
                return null;
            }
            docId = c.docId;
        }
        return docId;
    }

    private Map<String, Child> children(String parentDocId) {
        Map<String, Child> cached = listings.get(parentDocId);
        if (cached != null) {
            return cached;
        }
        Map<String, Child> result = new HashMap<>();
        Uri kids = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentDocId);
        String[] cols = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        try (Cursor c = getContentResolver().query(kids, cols, null, null, null)) {
            while (c != null && c.moveToNext()) {
                boolean dir = DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(2));
                String name = c.getString(1);
                result.put(name.toLowerCase(Locale.ROOT), new Child(c.getString(0), name, dir));
            }
        } catch (Exception e) {
            // Ordner nicht lesbar: leere Liste
        }
        listings.put(parentDocId, result);
        return result;
    }
}
