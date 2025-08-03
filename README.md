# RFID 
efenin kafesine kapı yaptım

1 gecekodu + 2 saat sürdü toplam.

# Sistem
- 1x esp32
- 2x RC522
- 1x Buton
- 1x Güç Rölesi

# Kullanım
- Google Sheets açıyoruz. "Logs" ve "UIDs" olarak 2 tab açıyoruz. İsimler makroda kullanıldığı için önemli.
- Uzantılar -> Apps Script (Apps Komut Dosyası)
```javascript
function doPost(e) {
  const params = JSON.parse(e.postData.contents);
  const ss     = SpreadsheetApp.getActive();
  const uidSheet = ss.getSheetByName("UIDs");
  const logSheet = ss.getSheetByName("Logs");

  if (params.operation && params.operation.trim().toUpperCase() === "LEARN") {
    const uid = params.uid;
    const existing = uidSheet.getRange("A:A").getValues().flat();
    if (existing.indexOf(uid) < 0) {
      uidSheet.appendRow([uid]);
    }
    return ContentService.createTextOutput("UID saved");
  }
  else if (params.operation && params.operation.trim().toUpperCase() === "LOG") {
    const { timestamp, uid, location, role } = params;
    logSheet.appendRow([timestamp, uid, location, role]);
    return ContentService.createTextOutput("Log added");
  }
  else {
    logSheet.appendRow([new Date(), "UNKNOWN OP:", params.operation]);
    return ContentService.createTextOutput("Unknown operation");
  }
}
```

- Bu makroyu yazıp web app olarak dağıtıyoruz. Dağıtım iznini herkese veriyoruz, Kendi hesabım ile çalıştırılacak seçeneğini işaretliyoruz.
- Web App dağıtım linkini main koddaki googleScriptURL değişkenine atıyoruz.  
  googleScriptURL örnek: https://script.google.com/macros/s/[ID]/exec
- Dosya -> Paylaş -> Web'de Yayınla'ya girip UIDs tabını CSV formatında bağlantı olarak yayınlıyoruz.
- Linki googleUIDSheetCSV değişkenine atıyoruz.  
  googleUIDSheetCSV örnek: https://docs.google.com/spreadsheets/d/e/[ANOTHERID]/pub?gid=1283621971&single=true&output=csv

# Uyarılar
- 12\. pine buton koymak lazım. basıldığında learn_uid ve door_handling modu arasında geçiş yapıyor.
- 5 Ghz'te çalışmıyor. ssid ve pass verirken 2.4ghz olmasına dikkat et.
- Admin kartlar hardcoded olarak kodun içinde duruyor. Hem güvenli değil hem de yeni admin kartların eklenmesini zorlaştırıyor.
- Sistemi kurduktan sonra Google Sheets'te çeşitli senaryolar için makrolar atanabilir. Yetki NONE loglandıysa kırmızı gözükmesi, çıkış yapmadan 2 kere giriş yapıldığında sarı gözükmesi vb.
- Röleyi test etmedim 5v'um yoktu.
