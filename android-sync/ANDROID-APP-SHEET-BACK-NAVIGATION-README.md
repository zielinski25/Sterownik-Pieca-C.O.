# Android app: Back from open sheet

The custom Compose sheets are drawn inside the main activity rather than opened as separate Android dialogs. The app did not intercept the system Back action, so Android could finish the activity while a sheet was open. The patch installs a BackHandler in the shared sheet component: Back dismisses the current sheet and reveals the page underneath. Main-page Back behavior remains unchanged.

After pulling the branch, from the repository root run:

```powershell
.\android-sync\Apply-Android-Sheet-Back-Navigation.ps1
```

Then rebuild and reinstall the Debug APK using the normal Android build process. The patch changes only Android source; it does not communicate with Firebase or the boiler.
