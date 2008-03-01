/*******************************************************************************
* This program and the accompanying materials
* are made available under the terms of the Common Public License v1.0
* which accompanies this distribution, and is available at 
* http://www.eclipse.org/legal/cpl-v10.html
* 
* Contributors:
*     Peter Smith
*******************************************************************************/

#include "Registry.h"
#include "Log.h"
#include "../java/JNI.h"

bool Registry::SetKeyAndValue(HKEY root, TCHAR* key, TCHAR* subkey, TCHAR* value)
{
	HKEY hKey;
	char szKeyBuf[1024] ;

	// Copy keyname into buffer.
	strcpy(szKeyBuf, key) ;

	// Add subkey name to buffer.
	if (subkey != NULL) {
		strcat(szKeyBuf, "\\") ;
		strcat(szKeyBuf, subkey ) ;
	}

	// Create and open key and subkey.
	long lResult = RegCreateKeyEx(root ,
		szKeyBuf, 
		0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS, NULL, 
		&hKey, NULL) ;
	if (lResult != ERROR_SUCCESS)
	{
		return FALSE ;
	}

	// Set the Value.
	if (value != NULL)
	{
		RegSetValueEx(hKey, NULL, 0, REG_SZ, 
			(BYTE *)value, 
			DWORD( 1+strlen(value) ) 
			) ;
	}

	RegCloseKey(hKey) ;
	return TRUE ;
}


bool Registry::SetValue(HKEY root, TCHAR* key, TCHAR* entry, TCHAR* value)
{
	HKEY hKey;

	// Create and open key and subkey.
	long lResult = RegOpenKeyEx(root,
		key, 
		0, 
		KEY_ALL_ACCESS, 
		&hKey) ;
	if (lResult != ERROR_SUCCESS)
	{
		return FALSE ;
	}

	// Set the Value.
	if (value != NULL)
	{
		RegSetValueEx(hKey, entry, 0, REG_SZ, 
			(BYTE *)value, 
			DWORD( 1+strlen(value) )
			) ;
	}

	RegCloseKey(hKey) ;

	return TRUE;
}

#ifndef REGISTRY_UTIL_ONLY
jlong Registry::OpenKey(JNIEnv* env, jobject self, jlong rootKey, jstring subKey)
{
	jboolean iscopy = false;
	const char* sk = subKey == NULL ? 0 : env->GetStringUTFChars(subKey, &iscopy);
	HKEY key;

	LONG result = RegOpenKeyEx((HKEY) rootKey, sk, 0, KEY_ALL_ACCESS, &key);

	if(subKey) env->ReleaseStringUTFChars(subKey, sk);

	if(result == ERROR_SUCCESS) {
		return (jlong) key;	
	} else {
		return 0;
	}
}

void Registry::CloseKey(JNIEnv* env, jobject self, jlong handle)
{
	if(handle == 0)
		return;
	RegCloseKey((HKEY) handle);
}

jobjectArray Registry::GetSubKeyNames(JNIEnv* env, jobject self, jlong handle)
{
	if(handle == 0)
		return 0;

	DWORD keyCount = 0;
	LONG result = RegQueryInfoKey((HKEY) handle, NULL, NULL, NULL, &keyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	if(result != ERROR_SUCCESS) {
		return NULL;
	}

	char tmp[MAX_PATH];

	jclass clazz = env->FindClass("java/lang/String");
	jobjectArray arr = env->NewObjectArray(keyCount, clazz, NULL);

	for(int i = 0; i < keyCount; i++) {
		DWORD size = MAX_PATH;
		RegEnumKeyEx((HKEY) handle, i, tmp, &size, 0, 0, 0, 0);	
		env->SetObjectArrayElement(arr, i, env->NewStringUTF(tmp));
	}

	return arr;
}

jobjectArray Registry::GetValueNames(JNIEnv* env, jobject self, jlong handle)
{
	if(handle == 0)
		return 0;

	DWORD valueCount = 0;
	LONG result = RegQueryInfoKey((HKEY) handle, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL);
	if(result != ERROR_SUCCESS) {
		return NULL;
	}

	char tmp[MAX_PATH];

	jclass clazz = env->FindClass("java/lang/String");
	jobjectArray arr = env->NewObjectArray(valueCount, clazz, NULL);

	for(int i = 0; i < valueCount; i++) {
		DWORD size = MAX_PATH;
		RegEnumValue((HKEY) handle, i, tmp, &size, 0, 0, 0, 0);	
		env->SetObjectArrayElement(arr, i, env->NewStringUTF(tmp));
	}

	return arr;
}

void Registry::DeleteKey(JNIEnv* env, jobject self, jlong handle)
{
	if(handle != 0)
		RegDeleteKey((HKEY) handle, 0);
}

void Registry::DeleteValue(JNIEnv* env, jobject self, jlong parent, jstring name)
{
	if(parent == 0)
		return;

	jboolean iscopy = false;
	const char* str = env->GetStringUTFChars(name, &iscopy);
	RegDeleteValue((HKEY) parent, str);
	env->ReleaseStringUTFChars(name, str);
}

jlong Registry::GetType(JNIEnv* env, jobject self, jlong parent, jstring name)
{
	if(parent == 0)
		return 0;

	jboolean iscopy = false;
	const char* str = env->GetStringUTFChars(name, &iscopy);
	DWORD type = 0;
	LONG result = RegQueryValueEx((HKEY) parent, str, NULL, &type, NULL, NULL);
	env->ReleaseStringUTFChars(name, str);

	if(result == ERROR_SUCCESS)
		return type;
	else 
		return 0;
}

jstring Registry::GetString(JNIEnv* env, jobject self, jlong parent, jstring name)
{
	if(parent == 0)
		return 0;

	jboolean iscopy = false;
	const char* str = env->GetStringUTFChars(name, &iscopy);
	DWORD type = 0;
	TCHAR buffer[4096];
	DWORD len = 4096;
	LONG result = RegQueryValueEx((HKEY) parent, str, 0, &type, (LPBYTE) buffer, &len);
	env->ReleaseStringUTFChars(name, str);

	if(result == ERROR_SUCCESS)
		return env->NewStringUTF((char *) buffer);
	else 
		return 0;
}

jbyteArray Registry::GetBinary(JNIEnv* env, jobject self, jlong parent, jstring name)
{
	if(parent == 0)
		return 0;

	jboolean iscopy = false;
	const char* str = env->GetStringUTFChars(name, &iscopy);
	DWORD type = 0;
	TCHAR buffer[4096];
	DWORD len = 4096;
	LONG result = RegQueryValueEx((HKEY) parent, str, 0, &type, (LPBYTE) buffer, &len);
	env->ReleaseStringUTFChars(name, str);

	if(result == ERROR_SUCCESS) {
		jbyteArray arr = env->NewByteArray(len);
		env->SetByteArrayRegion(arr, 0, len, (jbyte *)buffer);
		return arr;
	}
	else 
		return 0;
}

jlong Registry::GetDoubleWord(JNIEnv* env, jobject self, jlong parent, jstring name)
{
	if(parent == 0)
		return 0;

	jboolean iscopy = false;
	const char* str = env->GetStringUTFChars(name, &iscopy);
	DWORD type = 0;
	DWORD value = 0;
	LONG result = RegQueryValueEx((HKEY) parent, str, 0, &type, (LPBYTE) &value, NULL);
	env->ReleaseStringUTFChars(name, str);

	if(result == ERROR_SUCCESS)
		return value;
	else 
		return 0;
}

jlong Registry::GetDoubleWordLittleEndian(JNIEnv* env, jobject self, jlong parent, jstring name)
{
	if(parent == 0)
		return 0;

	jboolean iscopy = false;
	const char* str = env->GetStringUTFChars(name, &iscopy);
	DWORD type = 0;
	DWORD value = 0;
	LONG result = RegQueryValueEx((HKEY) parent, str, 0, &type, (LPBYTE) &value, NULL);
	env->ReleaseStringUTFChars(name, str);

	if(result == ERROR_SUCCESS)
		return value;
	else 
		return 0;
}

jlong Registry::GetDoubleWordBigEndian(JNIEnv* env, jobject self, jlong parent, jstring name)
{
	if(parent == 0)
		return 0;

	jboolean iscopy = false;
	const char* str = env->GetStringUTFChars(name, &iscopy);
	DWORD type = 0;
	DWORD value = 0;
	LONG result = RegQueryValueEx((HKEY) parent, str, 0, &type, (LPBYTE) &value, NULL);
	env->ReleaseStringUTFChars(name, str);

	if(result == ERROR_SUCCESS)
		return value;
	else 
		return 0;
}

jstring Registry::GetExpandedString(JNIEnv* env, jobject self, jlong parent, jstring name)
{
	if(parent == 0)
		return 0;

	jboolean iscopy = false;
	const char* str = env->GetStringUTFChars(name, &iscopy);
	DWORD type = 0;
	TCHAR buffer[4096];
	DWORD len = 4096;
	LONG result = RegQueryValueEx((HKEY) parent, str, 0, &type, (LPBYTE) buffer, &len);
	env->ReleaseStringUTFChars(name, str);

	if(result == ERROR_SUCCESS)
		return env->NewStringUTF((char *) buffer);
	else 
		return 0;
}

jobjectArray Registry::GetMultiString(JNIEnv* env, jobject self, jlong parent, jstring name)
{
	if(parent == 0)
		return 0;

	jboolean iscopy = false;
	const char* str = env->GetStringUTFChars(name, &iscopy);
	DWORD type = 0;
	TCHAR buffer[4096];
	DWORD len = 4096;
	LONG result = RegQueryValueEx((HKEY) parent, str, 0, &type, (LPBYTE) buffer, &len);
	env->ReleaseStringUTFChars(name, str);

	if(result == ERROR_SUCCESS) {
		return 0; // TODO convert result
	}
	else 
		return 0;
}

void Registry::SetString(JNIEnv* env, jobject self, jlong parent, jstring name, jstring value)
{
	if(parent == 0) return;
	jboolean iscopy = false;
	const char* nameStr = env->GetStringUTFChars(name, &iscopy);
	const char* valueStr = env->GetStringUTFChars(value, &iscopy);
	RegSetValue((HKEY) parent, nameStr, REG_SZ, valueStr, strlen(valueStr));
	env->ReleaseStringUTFChars(name, nameStr);
	env->ReleaseStringUTFChars(value, valueStr);
}

void Registry::SetBinary(JNIEnv* env, jobject self, jlong parent, jstring name, jarray value)
{
	if(parent == 0) return;
	jboolean iscopy = false;
	const char* nameStr = env->GetStringUTFChars(name, &iscopy);
	void* data = env->GetPrimitiveArrayCritical(value, &iscopy);
	RegSetValueEx((HKEY) parent, nameStr, 0, REG_BINARY, (const BYTE*) data, env->GetArrayLength(value));
	env->ReleasePrimitiveArrayCritical(value, data, 0);
	env->ReleaseStringUTFChars(name, nameStr);
}

void Registry::SetDoubleWord(JNIEnv* env, jobject self, jlong parent, jstring name, jlong value)
{
	if(parent == 0) return;
	jboolean iscopy = false;
	const char* nameStr = env->GetStringUTFChars(name, &iscopy);
	RegSetValueEx((HKEY) parent, nameStr, 0, REG_DWORD, (const BYTE*) &value, sizeof(DWORD));
	env->ReleaseStringUTFChars(name, nameStr);
}

void Registry::SetDoubleWordLittleEndian(JNIEnv* env, jobject self, jlong parent, jstring name, jlong value)
{
	if(parent == 0) return;
	jboolean iscopy = false;
	const char* nameStr = env->GetStringUTFChars(name, &iscopy);
	RegSetValueEx((HKEY) parent, nameStr, 0, REG_DWORD_LITTLE_ENDIAN, (const BYTE*) &value, sizeof(DWORD));
	env->ReleaseStringUTFChars(name, nameStr);
}

void Registry::SetDoubleWordBigEndian(JNIEnv* env, jobject self, jlong parent, jstring name, jlong value)
{
	if(parent == 0) return;
	jboolean iscopy = false;
	const char* nameStr = env->GetStringUTFChars(name, &iscopy);
	RegSetValueEx((HKEY) parent, nameStr, 0, REG_DWORD_BIG_ENDIAN, (const BYTE*) &value, sizeof(DWORD));
	env->ReleaseStringUTFChars(name, nameStr);
}

void Registry::SetMultiString(JNIEnv* env, jobject self, jlong parent, jstring name, jobjectArray value)
{
}

bool Registry::RegisterNatives(JNIEnv *env)
{
	Log::Info("Registering natives for Registry class\n");
	jclass clazz = env->FindClass("org/boris/winrun4j/RegistryKey");
	if(clazz == NULL) {
		Log::Warning("Could not find RegistryKey class\n");
		if(env->ExceptionOccurred())
			env->ExceptionClear();
		return false;
	}

	JNINativeMethod methods[20];
	methods[0].fnPtr = (void*) CloseKey;
	methods[0].name = "closeKeyHandle";
	methods[0].signature = "(J)V";
	methods[1].fnPtr = (void*) DeleteKey;
	methods[1].name = "deleteKey";
	methods[1].signature = "(J)V";
	methods[2].fnPtr = (void*) DeleteValue;
	methods[2].name = "deleteValue";
	methods[2].signature = "(JLjava/lang/String;)V";
	methods[3].fnPtr = (void*) GetBinary;
	methods[3].name = "getBinary";
	methods[3].signature = "(JLjava/lang/String;)[B";
	methods[4].fnPtr = (void*) GetDoubleWord;
	methods[4].name = "getDoubleWord";
	methods[4].signature = "(JLjava/lang/String;)J";
	methods[5].fnPtr = (void*) GetDoubleWordBigEndian;
	methods[5].name = "getDoubleWordBigEndian";
	methods[5].signature = "(JLjava/lang/String;)J";
	methods[6].fnPtr = (void*) GetDoubleWordLittleEndian;
	methods[6].name = "getDoubleWordLittleEndian";
	methods[6].signature = "(JLjava/lang/String;)J";
	methods[7].fnPtr = (void*) GetExpandedString;
	methods[7].name = "getExpandedString";
	methods[7].signature = "(JLjava/lang/String;)Ljava/lang/String;";
	methods[8].fnPtr = (void*) GetMultiString;
	methods[8].name = "getMultiString";
	methods[8].signature = "(JLjava/lang/String;)[Ljava/lang/String;";
	methods[9].fnPtr = (void*) GetString;
	methods[9].name = "getString";
	methods[9].signature = "(JLjava/lang/String;)Ljava/lang/String;";
	methods[10].fnPtr = (void*) GetSubKeyNames;
	methods[10].name = "getSubKeyNames";
	methods[10].signature = "(J)[Ljava/lang/String;";
	methods[11].fnPtr = (void*) GetType;
	methods[11].name = "getType";
	methods[11].signature = "(JLjava/lang/String;)J";
	methods[12].fnPtr = (void*) GetValueNames;
	methods[12].name = "getValueNames";
	methods[12].signature = "(J)[Ljava/lang/String;";
	methods[13].fnPtr = (void*) OpenKey;
	methods[13].name = "openKeyHandle";
	methods[13].signature = "(JLjava/lang/String;)J";
	methods[14].fnPtr = (void*) SetBinary;
	methods[14].name = "setBinary";
	methods[14].signature = "(JLjava/lang/String;[B)V";
	methods[15].fnPtr = (void*) SetDoubleWord;
	methods[15].name = "setDoubleWord";
	methods[15].signature = "(JLjava/lang/String;J)V";
	methods[16].fnPtr = (void*) SetDoubleWordBigEndian;
	methods[16].name = "setDoubleWordBigEndian";
	methods[16].signature = "(JLjava/lang/String;J)V";
	methods[17].fnPtr = (void*) SetDoubleWordLittleEndian;
	methods[17].name = "setDoubleWordLittleEndian";
	methods[17].signature = "(JLjava/lang/String;J)V";
	methods[18].fnPtr = (void*) SetMultiString;
	methods[18].name = "setMultiString";
	methods[18].signature = "(JLjava/lang/String;[Ljava/lang/String;)V";
	methods[19].fnPtr = (void*) SetString;
	methods[19].name = "setString";
	methods[19].signature = "(JLjava/lang/String;Ljava/lang/String;)V";
	
	env->RegisterNatives(clazz, methods, 20);
	if(env->ExceptionOccurred()) {
		char* msg = JNI::GetExceptionMessage(env);
		Log::Error(msg);
		env->ExceptionClear();
	}
	return true;
}
#endif