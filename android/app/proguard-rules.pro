# SDL's JNI entry points are covered by its upstream rules in build.gradle.
# This callback was added in SDL before its upstream keep list was updated.
-keepclassmembers,allowoptimization class org.libsdl.app.SDLActivity {
    static java.lang.String getDeviceFormFactor();
}

# Sil-More also looks up this callback by name from sdl-settings.c.
-keepclassmembers,allowoptimization class com.silqh.silmore.SilMoreActivity {
    public void requestGameOrientation(boolean);
}
