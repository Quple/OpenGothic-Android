package org.opengothic.app;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.widget.Toast;

/** The answer of the system installer to the update that {@link AppUpdate} has committed. */
public final class UpdateReceiver extends BroadcastReceiver {
    @Override
    @SuppressWarnings("deprecation")
    public void onReceive(Context context, Intent intent) {
        int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
        switch (status) {
            case PackageInstaller.STATUS_PENDING_USER_ACTION: {
                // the system asks the player to confirm: its own screen is shown on top of the launcher
                Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
                if (confirm != null) {
                    confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    context.startActivity(confirm);
                }
                break;
            }
            case PackageInstaller.STATUS_SUCCESS:
                // the application is replaced, the process ends
                break;
            default: {
                String message = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
                AppUpdate.error = message != null ? message : ("status " + status);
                // the player has cancelled it: nothing to tell
                if (status != PackageInstaller.STATUS_FAILURE_ABORTED)
                    Toast.makeText(context, Texts.t("The update failed: ", "Aktualizacja się nie udała: ") + AppUpdate.error,
                            Toast.LENGTH_LONG).show();
                break;
            }
        }
    }
}
