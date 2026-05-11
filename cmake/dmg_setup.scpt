-- AppleScript: sets the DMG window layout for the drag-and-drop installer.
-- CPack calls this via CPACK_DMG_DS_STORE_SETUP_SCRIPT.
on run (volumeName)
    set theVolume to POSIX file ("/Volumes/" & volumeName)
    tell application "Finder"
        tell disk (volumeName as string)
            open
            set current view of container window to icon view
            set toolbar visible of container window to false
            set statusbar visible of container window to false
            set bounds of container window to {400, 100, 900, 400}

            set theViewOptions to the icon view options of container window
            set arrangement of theViewOptions to not arranged
            set icon size of theViewOptions to 96

            -- Position the .app and the Applications alias
            set position of item "PhysicsSimulationStudio.app" of container window to {130, 150}
            set position of item "Applications" of container window to {370, 150}

            -- Create Applications alias if it doesn't exist
            if not (exists item "Applications" of container window) then
                make new alias file at theVolume to POSIX file "/Applications" with properties {name:"Applications"}
            end if

            update without registering applications
            delay 2
        end tell
    end tell
end run
