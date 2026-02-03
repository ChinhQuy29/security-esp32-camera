import { useEffect } from "react";
import { Alert, Platform } from "react-native";
import * as Device from "expo-device";
import * as Notifications from "expo-notifications";
import { View, Text, StyleSheet, Button } from "react-native";

// Setup notification handler (how alerts look when app is open)
Notifications.setNotificationHandler({
  handleNotification: async () => ({
    shouldShowAlert: true,
    shouldPlaySound: true,
    shouldSetBadge: false,
  }),
});

export default function App() {
  useEffect(() => {
    registerForPushNotifications();
  }, []);

  async function registerForPushNotifications() {
    // 1. Check if physical device
    if (!Device.isDevice) {
      console.log("Must use physical device for Push Notifications");
      return;
    }

    // 2. Request Permissions
    const { status: existingStatus } =
      await Notifications.getPermissionsAsync();
    let finalStatus = existingStatus;
    if (existingStatus !== "granted") {
      const { status } = await Notifications.requestPermissionsAsync();
      finalStatus = status;
    }
    if (finalStatus !== "granted") {
      Alert.alert(
        "Permission needed",
        "Enable push notifications in settings!",
      );
      return;
    }

    // 3. Get the Device Token (FCM Token)
    // IMPORTANT: Get the DevicePushToken, NOT the ExpoPushToken
    const tokenData = await Notifications.getDevicePushTokenAsync();
    const token = tokenData.data;
    console.log("My FCM Token:", token);

    // 4. Send Token to Your Python Server
    try {
      await fetch("http://192.168.1.123:8000/api/register-token", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ token: token }),
      });
      console.log("✅ Token registered with Backend!");
    } catch (error) {
      console.error("❌ Failed to register token:", error);
    }
  }

  return (
    <View style={styles.container}>
      {/* 1. Header Icon/Text */}
      <Text style={styles.icon}>🛡️</Text>
      <Text style={styles.title}>Security System</Text>

      {/* 2. Status Indicator */}
      <View style={styles.statusContainer}>
        <View style={styles.dot} />
        <Text style={styles.statusText}>Listening for alerts...</Text>
      </View>

      {/* 3. Debug Button - Triggers an immediate local alert */}
      <View style={styles.buttonContainer}>
        <Button
          title="Test Notification Now"
          onPress={async () => {
            await Notifications.scheduleNotificationAsync({
              content: {
                title: "Test Alert 🔔",
                body: "If you see this, notifications are working!",
              },
              trigger: null, // Send immediately
            });
          }}
        />
      </View>
    </View>
  );
}
