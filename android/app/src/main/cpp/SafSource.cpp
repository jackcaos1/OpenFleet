#include "SafSource.h"

#include <sys/stat.h>
#include <unistd.h>

#include <string>

namespace fleet {

namespace {

class SafSource : public FileSource {
 public:
  SafSource(JavaVM* vm, jobject activity) : vm_(vm), activity_(activity) {
    JNIEnv* env = Env();
    jclass cls = env->GetObjectClass(activity_);
    list_dir_ = env->GetMethodID(cls, "listDir", "(Ljava/lang/String;)[Ljava/lang/String;");
    exists_ = env->GetMethodID(cls, "exists", "(Ljava/lang/String;)Z");
    open_fd_ = env->GetMethodID(cls, "openFd", "(Ljava/lang/String;)I");
    env->DeleteLocalRef(cls);
  }

  bool ReadFile(const std::string& path, std::vector<uint8_t>* out) override {
    JNIEnv* env = Env();
    jstring js = env->NewStringUTF(path.c_str());
    const jint fd = env->CallIntMethod(activity_, open_fd_, js);
    env->DeleteLocalRef(js);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      return false;
    }
    if (fd < 0) return false;
    struct stat st;
    bool ok = false;
    if (fstat(fd, &st) == 0 && st.st_size >= 0) {
      out->resize(static_cast<size_t>(st.st_size));
      size_t done = 0;
      while (done < out->size()) {
        const ssize_t n = read(fd, out->data() + done, out->size() - done);
        if (n <= 0) break;
        done += static_cast<size_t>(n);
      }
      ok = done == out->size();
    }
    close(fd);
    return ok;
  }

  bool Exists(const std::string& path) override {
    JNIEnv* env = Env();
    jstring js = env->NewStringUTF(path.c_str());
    const jboolean r = env->CallBooleanMethod(activity_, exists_, js);
    env->DeleteLocalRef(js);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      return false;
    }
    return r == JNI_TRUE;
  }

  std::vector<std::string> ListDir(const std::string& dir) override {
    std::vector<std::string> names;
    JNIEnv* env = Env();
    jstring js = env->NewStringUTF(dir.c_str());
    jobjectArray arr = static_cast<jobjectArray>(env->CallObjectMethod(activity_, list_dir_, js));
    env->DeleteLocalRef(js);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      return names;
    }
    if (!arr) return names;
    const jsize n = env->GetArrayLength(arr);
    for (jsize i = 0; i < n; ++i) {
      jstring s = static_cast<jstring>(env->GetObjectArrayElement(arr, i));
      const char* c = env->GetStringUTFChars(s, nullptr);
      names.emplace_back(c);
      env->ReleaseStringUTFChars(s, c);
      env->DeleteLocalRef(s);
    }
    env->DeleteLocalRef(arr);
    return names;
  }

 private:
  JNIEnv* Env() {
    JNIEnv* env = nullptr;
    vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    return env;
  }

  JavaVM* vm_;
  jobject activity_;
  jmethodID list_dir_ = nullptr;
  jmethodID exists_ = nullptr;
  jmethodID open_fd_ = nullptr;
};

}  // namespace

std::unique_ptr<FileSource> OpenSafFolder(JavaVM* vm, jobject activity) { return std::make_unique<SafSource>(vm, activity); }

}  // namespace fleet
