# Contracts, latest owner directive overrides historical firmware
Package com.civichud.maintenance. Three Gradle modules :domain (Kotlin JVM), :data (Android Room), :app (Compose/service).

Domain models in com.civichud.maintenance.domain:
Vehicle(id:Long=1,name:String,baseOdometerKm:Double,accumulatedKm:Double,calibratedAt:Long,isCalibrated:Boolean,adapterAddress:String), estimatedOdometerKm getter.
MaintenanceRule(id:Long,name:String,intervalKm:Double,intervalMonths:Int,lastServiceOdometerKm:Double,lastServiceDate:java.time.LocalDate,warningKm:Double,warningDays:Long,cycle:Long).
ServiceLog(id:Long,itemId:Long,itemName:String,odometerKm:Double,performedAt:Long,latitude:Double?,longitude:Double?,locationText:String,mechanicName:String,phone:String,cost:Double?,notes:String).
ServiceDraft(latitude:Double?,longitude:Double?,locationText:String,mechanicName:String,phone:String,cost:Double?,notes:String).
MaintenanceResult(rule:MaintenanceRule,remainingKm:Double?,remainingDays:Long?,usedFraction:Double,status:MaintenanceStatus enum NORMAL/APPROACHING/OVERDUE).
MaintenanceCalculator.evaluate(rule, odometerKm, today):MaintenanceResult.
TrackingStatus enum STOPPED/SCANNING/CONNECTING/INITIALIZING/CONNECTED/DATA_STALE/DISCONNECTED/RECONNECT_WAIT.
TrackingTelemetry(status:TrackingStatus,speedKmh:Double?,sessionDistanceKm:Double,accumulatedKm:Double,detail:String,retryAttempt:Int=0,nextRetryMs:Long=0).

:data class com.civichud.maintenance.data.TrackerStore(context:Context,simulation:Boolean=false), flows vehicle:Flow<Vehicle?>,rules:Flow<List<MaintenanceRule>>,history:Flow<List<ServiceLog>>.
Suspend APIs initialize(),currentVehicle():Vehicle,checkpoint(accumulatedKm:Double),calibrate(actualKm:Double),saveAdapter(address:String),saveRule(rule:MaintenanceRule),deleteRule(id:Long),logService(ruleId:Long,odometerKm:Double,draft:ServiceDraft),markNotified(ruleId:Long,cycle:Long,status:MaintenanceStatus):Boolean (true only if not sent before; transaction/dedup).
Room filename vehicle_tracker.db vs vehicle_tracker_simulation.db. SyncId UUID, updatedAt epoch, isDirty in each persisted entity. Initial defaults have last service date at vehicle setup only; UI must describe starting baseline, not claim actual historic services. Calibration sets base actual and accumulated0 (service ensures pending distance handled first); history unchanged.

:app UI in com.civichud.maintenance.ui:
TrackerUiState(vehicle:Vehicle?,rules:List<MaintenanceResult>,history:List<ServiceLog>,telemetry:TrackingTelemetry,simulation:Boolean,devices:List<PairedObdDevice>,message:String,gpsBusy:Boolean,gpsLocation:CapturedLocation?).
PairedObdDevice(name:String,address:String), CapturedLocation(latitude:Double,longitude:Double,address:String).
TrackerActions(onRefreshDevices:()->Unit,onConnect:(String)->Unit,onStartSimulation:()->Unit,onStop:()->Unit,onCalibrate:(Double)->Unit,onSaveRule:(MaintenanceRule)->Unit,onDeleteRule:(Long)->Unit,onLogService:(Long,ServiceDraft)->Unit,onCaptureLocation:()->Unit,onClearMessage:()->Unit).
@Composable fun TrackerScreen(state:TrackerUiState,actions:TrackerActions). UI consumes only these values/callbacks; no Android Bluetooth, parser or DAO calls.

Root handles MainActivity, graph, real/simulation transport, protocol, foreground service, permissions, GPS, workers. Debug manifest only exported receiver protected android.permission.DUMP (shell). Real and simulated mileage never mixed. Real Bluetooth chosen only by deliberate connect selection. No hardcoded vehicle mileage or fake data in production. Defaults must require initial calibration; app label เลขไมล์ประมาณ.
