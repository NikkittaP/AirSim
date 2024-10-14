#include "CameraManager.h"
#include "GameFramework/PlayerController.h"
#include "AirBlueprintLib.h"

ACameraManager::ACameraManager()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create a spring arm component for our chase camera
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 34.0f));
    SpringArm->SetWorldRotation(FRotator(-20.0f, 0.0f, 0.0f));
    SpringArm->TargetArmLength = 125.0f;
    SpringArm->bEnableCameraLag = false;
    SpringArm->bEnableCameraRotationLag = false;
    SpringArm->CameraRotationLagSpeed = 10.0f;
    SpringArm->bInheritPitch = false;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;
    SpringArm->bDoCollisionTest = false;
}

void ACameraManager::BeginPlay()
{
    Super::BeginPlay();
}

void ACameraManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_MANUAL) {
        manual_pose_controller_->updateActorPose(DeltaTime);
    }
    else if (mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_SPRINGARM_CHASE) {
        //do nothing, spring arm is pulling the camera with it
    }
    else if (mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_FLY_WITH_ME) {
        UAirBlueprintLib::FollowActor(SpringArm->GetOwner(), follow_actor_, initial_ground_obs_offset_, ext_obs_fixed_z_);
    }
    else if (mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_NODISPLAY) {
        //do nothing, we have camera turned off
    }
    else { //make camera move in desired way
        UAirBlueprintLib::FollowActor(ExternalCamera, follow_actor_, initial_ground_obs_offset_, ext_obs_fixed_z_);
    }
}

ECameraManagerMode ACameraManager::getMode()
{
    return mode_;
}

void ACameraManager::initializeForBeginPlay(ECameraManagerMode view_mode,
    AActor* follow_actor, APIPCamera* fpv_camera, APIPCamera* front_camera, APIPCamera* back_camera)
{
    manual_pose_controller_ = NewObject<UManualPoseController>(this, "CameraManager_ManualPoseController");
    manual_pose_controller_->initializeForPlay();

    setupInputBindings();

    mode_ = view_mode;

    switchPossession(follow_actor, fpv_camera, front_camera, back_camera);
}

void ACameraManager::attachSpringArm(bool attach)
{
    if (attach) {
        //If we do have actor to follow AND don't have sprint arm attached to that actor, we will attach it
        if (follow_actor_ && ExternalCamera->GetRootComponent()->GetAttachParent() != SpringArm) {
            //For car, we want a bit of camera lag, as that is customary of racing video games
            //If the lag is missing, the camera will also occasionally shake.
            //But, lag is not desired when piloting a drone
            SpringArm->bEnableCameraRotationLag = camera_rotation_lag_enabled_;

            //attach spring arm to actor
            SpringArm->AttachToComponent(follow_actor_->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 34.0f));

            //remember current parent for external camera. Later when we remove external
            //camera from spring arm, we will attach it back to its last parent
            last_parent_ = ExternalCamera->GetRootComponent()->GetAttachParent();
            ExternalCamera->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            //now attach camera to spring arm
            ExternalCamera->AttachToComponent(SpringArm, FAttachmentTransformRules::KeepRelativeTransform);
        }

        //For car, we need to move the camera back a little more than for a drone.
        //Otherwise, the camera will be stuck inside the car
        ExternalCamera->SetActorRelativeLocation(FVector(follow_distance_ * 100.0f, 0.0f, 0.0f));
        ExternalCamera->SetActorRelativeRotation(FRotator(10.0f, 0.0f, 0.0f));
        //ExternalCamera->bUsePawnControlRotation = false;
    }
    else { //detach
        if (ExternalCamera->GetRootComponent()->GetAttachParent() == SpringArm) {
            ExternalCamera->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
            ExternalCamera->AttachToComponent(last_parent_, FAttachmentTransformRules::KeepRelativeTransform);
        }
    }
}

void ACameraManager::setMode(ECameraManagerMode mode)
{
    switch (mode) {
    case ECameraManagerMode::CAMERA_MANAGER_MODE_FPV:
        inputEventFpvView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_FLY_WITH_ME:
        inputEventFlyWithView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_GROUND_OBSERVER:
        inputEventGroundView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_MANUAL:
        inputEventManualView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_SPRINGARM_CHASE:
        inputEventSpringArmChaseView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_BACKUP:
        inputEventBackupView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_NODISPLAY:
        inputEventNoDisplayView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_FRONT:
        inputEventFrontView();
        break;
    default:
        //other modes don't need special setup
        break;
    }
}

bool ACameraManager::getFPVGimbalEnabled()
{
    return getFpvCamera()->getGimbalEnabled();
}

void ACameraManager::setFPVGimbalEnabled(bool enabled)
{
    getFpvCamera()->setGimbalEnabled(enabled);
}

void ACameraManager::setFPVGimbalStabilization(float stabilization)
{
    getFpvCamera()->setGimbalStabilization(stabilization);
}

float ACameraManager::getFPVGimbalPitch()
{
    return getFpvCamera()->getGimbalRotatorPitch();
}

void ACameraManager::setFPVGimbalPitch(float pitch)
{
    getFpvCamera()->setGimbalRotatorPitch(pitch);
}

void ACameraManager::switchPossession(AActor* follow_actor, APIPCamera* fpv_camera, APIPCamera* front_camera, APIPCamera* back_camera)
{
    if (mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_SPRINGARM_CHASE || mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_FLY_WITH_ME) {
        attachSpringArm(false);
    }

    follow_actor_ = follow_actor;
    fpv_camera_ = fpv_camera;
    fpv_camera_->setDronePawn(follow_actor_);
    front_camera_ = front_camera;
    backup_camera_ = back_camera;
    camera_start_location_ = ExternalCamera->GetActorLocation();
    camera_start_rotation_ = ExternalCamera->GetActorRotation();
    initial_ground_obs_offset_ = camera_start_location_ -
        (follow_actor_ ? follow_actor_->GetActorLocation() : FVector::ZeroVector);

    //set initial view mode
    switch (mode_) {
    case ECameraManagerMode::CAMERA_MANAGER_MODE_FLY_WITH_ME:
        inputEventFlyWithView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_FPV:
        inputEventFpvView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_GROUND_OBSERVER:
        inputEventGroundView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_MANUAL:
        inputEventManualView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_SPRINGARM_CHASE:
        inputEventSpringArmChaseView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_BACKUP:
        inputEventBackupView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_NODISPLAY:
        inputEventNoDisplayView();
        break;
    case ECameraManagerMode::CAMERA_MANAGER_MODE_FRONT:
        inputEventFrontView();
        break;
    default:
        throw std::out_of_range("Unsupported view mode specified in CameraManager::initializeForBeginPlay");
    }
}

void ACameraManager::setupInputBindings()
{
    UAirBlueprintLib::EnableInput(this);

    UAirBlueprintLib::BindActionToKey("inputEventFpvView", EKeys::F, this, &ACameraManager::inputEventFpvView);
    UAirBlueprintLib::BindActionToKey("inputEventFlyWithView", EKeys::B, this, &ACameraManager::inputEventFlyWithView);
    UAirBlueprintLib::BindActionToKey("inputEventGroundView", EKeys::Backslash, this, &ACameraManager::inputEventGroundView);
    UAirBlueprintLib::BindActionToKey("inputEventManualView", EKeys::M, this, &ACameraManager::inputEventManualView);
    UAirBlueprintLib::BindActionToKey("inputEventSpringArmChaseView", EKeys::Slash, this, &ACameraManager::inputEventSpringArmChaseView);
    UAirBlueprintLib::BindActionToKey("inputEventBackupView", EKeys::K, this, &ACameraManager::inputEventBackupView);
    UAirBlueprintLib::BindActionToKey("inputEventNoDisplayView", EKeys::Hyphen, this, &ACameraManager::inputEventNoDisplayView);
    UAirBlueprintLib::BindActionToKey("inputEventFrontView", EKeys::I, this, &ACameraManager::inputEventFrontView);
}

void ACameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    manual_pose_controller_ = nullptr;
    SpringArm = nullptr;
    ExternalCamera = nullptr;
}

APIPCamera* ACameraManager::getFpvCamera() const
{
    return fpv_camera_;
}

APIPCamera* ACameraManager::getExternalCamera() const
{
    return ExternalCamera;
}

APIPCamera* ACameraManager::getBackupCamera() const
{
    return backup_camera_;
}

void ACameraManager::inputEventSpringArmChaseView()
{
    if (ExternalCamera) {
        //SpringArm->TargetArmLength = 125.0f;
        setModeInternal(ECameraManagerMode::CAMERA_MANAGER_MODE_SPRINGARM_CHASE);
        ExternalCamera->showToScreen();
        disableCameras(true, true, false, true);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void ACameraManager::inputEventGroundView()
{
    if (ExternalCamera) {
        setModeInternal(ECameraManagerMode::CAMERA_MANAGER_MODE_GROUND_OBSERVER);
        ExternalCamera->showToScreen();
        disableCameras(true, true, false, true);
        ext_obs_fixed_z_ = true;
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void ACameraManager::inputEventManualView()
{
    if (ExternalCamera) {
        setModeInternal(ECameraManagerMode::CAMERA_MANAGER_MODE_MANUAL);
        ExternalCamera->showToScreen();
        disableCameras(true, true, false, true);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void ACameraManager::inputEventNoDisplayView()
{
    if (ExternalCamera) {
        setModeInternal(ECameraManagerMode::CAMERA_MANAGER_MODE_NODISPLAY);
        disableCameras(true, true, true, true);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void ACameraManager::inputEventBackupView()
{
    if (backup_camera_) {
        setModeInternal(ECameraManagerMode::CAMERA_MANAGER_MODE_BACKUP);
        backup_camera_->showToScreen();
        disableCameras(true, false, true, true);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "backup_camera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void ACameraManager::inputEventFrontView()
{
    if (front_camera_) {
        setModeInternal(ECameraManagerMode::CAMERA_MANAGER_MODE_FRONT);
        front_camera_->showToScreen();
        disableCameras(true, true, true, false);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "backup_camera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void ACameraManager::inputEventFlyWithView()
{
    if (ExternalCamera) {
        //SpringArm->TargetArmLength = 3 * 125.0f;
        setModeInternal(ECameraManagerMode::CAMERA_MANAGER_MODE_FLY_WITH_ME);
        ExternalCamera->showToScreen();

        //if (follow_actor_)
        //    ExternalCamera->SetActorLocationAndRotation(
        //        follow_actor_->GetActorLocation() + initial_ground_obs_offset_, camera_start_rotation_);
        disableCameras(true, true, false, true);
        ext_obs_fixed_z_ = false;
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void ACameraManager::inputEventFpvView()
{
    if (fpv_camera_) {
        setModeInternal(ECameraManagerMode::CAMERA_MANAGER_MODE_FPV);
        fpv_camera_->showToScreen();
        disableCameras(false, true, true, true);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "fpv_camera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void ACameraManager::disableCameras(bool fpv, bool backup, bool external, bool front)
{
    if (fpv && fpv_camera_)
        fpv_camera_->disableMain();
    if (backup && backup_camera_)
        backup_camera_->disableMain();
    if (external && ExternalCamera)
        ExternalCamera->disableMain();
    if (front && front_camera_)
        front_camera_->disableMain();
}

void ACameraManager::notifyViewModeChanged()
{
    bool nodisplay = ECameraManagerMode::CAMERA_MANAGER_MODE_NODISPLAY == mode_;

    UWorld* world = GetWorld();
    UGameViewportClient* gameViewport = world->GetGameViewport();
    gameViewport->bDisableWorldRendering = nodisplay;
}

void ACameraManager::setModeInternal(ECameraManagerMode mode)
{
    FVector CurrentCameraWorldLocation = camera_start_location_;
    FRotator CurrentCameraWorldRotation = camera_start_rotation_;

    { //first remove any settings done by previous mode

        if (mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_FPV)
        {
            CurrentCameraWorldLocation = fpv_camera_->GetActorLocation();
            CurrentCameraWorldRotation = fpv_camera_->GetActorRotation();
        }

        //detach spring arm
        if ((mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_SPRINGARM_CHASE &&
            mode != ECameraManagerMode::CAMERA_MANAGER_MODE_SPRINGARM_CHASE) || (mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_FLY_WITH_ME &&
                mode != ECameraManagerMode::CAMERA_MANAGER_MODE_FLY_WITH_ME)) {
            CurrentCameraWorldLocation = ExternalCamera->GetActorLocation();
            CurrentCameraWorldRotation = ExternalCamera->GetActorRotation();
            attachSpringArm(false);
        }

        // Re-enable rendering
        if (mode_ == ECameraManagerMode::CAMERA_MANAGER_MODE_NODISPLAY &&
            mode != ECameraManagerMode::CAMERA_MANAGER_MODE_NODISPLAY) {
            UAirBlueprintLib::enableViewportRendering(this, true);
        }

        //Remove any existing key bindings for manual mode
        if (mode != ECameraManagerMode::CAMERA_MANAGER_MODE_MANUAL) {
            if (ExternalCamera != nullptr && manual_pose_controller_->getActor() == ExternalCamera) {

                manual_pose_controller_->setActor(nullptr);
            }
            //else someone else is bound to manual pose controller, leave it alone
        }
    }

    { //perform any settings to enter in to this mode

        switch (mode) {
        case ECameraManagerMode::CAMERA_MANAGER_MODE_MANUAL:
            //if new mode is manual mode then add key bindings
            ExternalCamera->SetActorLocationAndRotation(CurrentCameraWorldLocation, CurrentCameraWorldRotation);
            manual_pose_controller_->setActor(ExternalCamera);
            break;
        case ECameraManagerMode::CAMERA_MANAGER_MODE_SPRINGARM_CHASE:
        case ECameraManagerMode::CAMERA_MANAGER_MODE_FLY_WITH_ME:
            //if we switched to spring arm mode then attach to spring arm (detachment was done earlier in method)
            attachSpringArm(true);
            break;
        case ECameraManagerMode::CAMERA_MANAGER_MODE_NODISPLAY:
            UAirBlueprintLib::enableViewportRendering(this, false);
            break;
        default:
            //other modes don't need special setup
            break;
        }
    }

    //make switch official
    mode_ = mode;
}
