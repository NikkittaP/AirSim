#pragma once

#include "CoreMinimal.h"
#include "PawnSimApi.h"
#include "PIPCamera.h"
#include "GameFramework/Actor.h"
#include "ManualPoseController.h"
#include "common/common_utils/Utils.hpp"
#include "GameFramework/SpringArmComponent.h"
#include "CameraManager.generated.h"

UENUM(BlueprintType)
enum class ECameraManagerMode : uint8
{
    CAMERA_MANAGER_MODE_FPV = 0 UMETA(DisplayName = "FPV"),
    CAMERA_MANAGER_MODE_GROUND_OBSERVER = 1 UMETA(DisplayName = "GroundObserver"),
    CAMERA_MANAGER_MODE_FLY_WITH_ME = 2 UMETA(DisplayName = "FlyWithMe"),
    CAMERA_MANAGER_MODE_MANUAL = 3 UMETA(DisplayName = "Manual"),
    CAMERA_MANAGER_MODE_SPRINGARM_CHASE = 4 UMETA(DisplayName = "SpringArmChase"),
    CAMERA_MANAGER_MODE_BACKUP = 5 UMETA(DisplayName = "Backup"),
    CAMERA_MANAGER_MODE_NODISPLAY = 6 UMETA(DisplayName = "No Display"),
    CAMERA_MANAGER_MODE_FRONT = 7 UMETA(DisplayName = "Front")
};

UCLASS()
class AIRSIM_API ACameraManager : public AActor
{
    GENERATED_BODY()

public:
    /** Spring arm that will offset the camera */
    UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* SpringArm;

    UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    APIPCamera* ExternalCamera;

public:
    void inputEventFpvView();
    void inputEventGroundView();
    void inputEventManualView();
    void inputEventFlyWithView();
    void inputEventSpringArmChaseView();
    void inputEventBackupView();
    void inputEventNoDisplayView();
    void inputEventFrontView();

public:
    ACameraManager();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Modes")
    ECameraManagerMode getMode();
    UFUNCTION(BlueprintCallable, Category = "Modes")
    void setMode(ECameraManagerMode mode);

    UFUNCTION(BlueprintCallable, Category = "Properties")
    bool getFPVGimbalEnabled();
    UFUNCTION(BlueprintCallable, Category = "Properties")
    void setFPVGimbalEnabled(bool enabled);
    UFUNCTION(BlueprintCallable, Category = "Properties")
    void setFPVGimbalStabilization(float stabilization);
    UFUNCTION(BlueprintCallable, Category = "Properties")
    float getFPVGimbalPitch();
    UFUNCTION(BlueprintCallable, Category = "Properties")
    void setFPVGimbalPitch(float pitch);

    void initializeForBeginPlay(ECameraManagerMode view_mode,
                                AActor* follow_actor, APIPCamera* fpv_camera, APIPCamera* front_camera, APIPCamera* back_camera);

    APIPCamera* getFpvCamera() const;
    APIPCamera* getExternalCamera() const;
    APIPCamera* getBackupCamera() const;
    void setFollowDistance(const int follow_distance) { this->follow_distance_ = follow_distance; }
    void setCameraRotationLagEnabled(const bool lag_enabled) { this->camera_rotation_lag_enabled_ = lag_enabled; }

    void switchPossession(AActor* follow_actor, APIPCamera* fpv_camera, APIPCamera* front_camera, APIPCamera* back_camera);

private:
    void setupInputBindings();
    void attachSpringArm(bool attach);
    void disableCameras(bool fpv, bool backup, bool external, bool front);
    void notifyViewModeChanged();
    void setModeInternal(ECameraManagerMode mode);

private:
    typedef common_utils::Utils Utils;

    APIPCamera* fpv_camera_;
    APIPCamera* backup_camera_;
    APIPCamera* front_camera_;
    AActor* follow_actor_;

    USceneComponent* last_parent_ = nullptr;

    ECameraManagerMode mode_;
    UPROPERTY()
    UManualPoseController* manual_pose_controller_;

    FVector camera_start_location_;
    FVector initial_ground_obs_offset_;
    FRotator camera_start_rotation_;
    bool ext_obs_fixed_z_;
    int follow_distance_;
    bool camera_rotation_lag_enabled_;
};