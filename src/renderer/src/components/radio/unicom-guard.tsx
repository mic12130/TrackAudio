import useRadioState, { RadioType } from '@renderer/store/radioStore';
import '../../style/UnicomGuard.scss';
import { useEffect, useMemo, useState } from 'react';
import clsx from 'clsx';
import useSessionStore from '@renderer/store/sessionStore';
import useErrorStore from '@renderer/store/errorStore';
import { GuardFrequency, UnicomFrequency } from '../../../../shared/common';
import { useMediaQuery } from 'react-responsive';
import { VolumeX } from 'lucide-react';

const UnicomGuardBar = () => {
  const [radios, setRadioState, addRadio, removeRadio, setOutputVolume] = useRadioState((state) => [
    state.radios,
    state.setRadioState,
    state.addRadio,
    state.removeRadio,
    state.setOutputVolume
  ]);
  const [isConnected, isAtc] = useSessionStore((state) => [state.isConnected, state.isAtc]);

  const isReducedSize = useMediaQuery({ maxWidth: '895px' });

  const [localUnicomStationVolume, setLocalUnicomStationVolume] = useState(100);

  const [showingUnicomBar] = useRadioState((state) => [state.showingUnicomBar]);

  const postError = useErrorStore((state) => state.postError);

  const unicom = useMemo(() => {
    return radios.find((radio) => radio.frequency === UnicomFrequency);
  }, [radios, isConnected]);

  const guard = useMemo(() => {
    return radios.find((radio) => radio.frequency === GuardFrequency);
  }, [radios, isConnected]);

  const reAddRadio = (radio: RadioType, eventType: 'RX' | 'TX' | 'SPK') => {
    const radioName =
      radio.frequency === UnicomFrequency
        ? 'UNICOM'
        : radio.frequency === GuardFrequency
          ? 'GUARD'
          : 'INVALIDUNIGUARD';

    window.api
      .addFrequency(radio.frequency, radioName)
      .then((ret) => {
        if (!ret) {
          postError(`Failed to re-add ${radioName} frequency`);
          return;
        }
        if (eventType === 'RX') {
          clickRx(radio, true);
        }
        if (eventType === 'TX') {
          clickTx(radio, true);
        }
        if (eventType === 'SPK') {
          clickSpK(radio, true);
        }
      })
      .catch((err: unknown) => {
        console.error(err);
      });
  };

  const clickRx = (radio: RadioType | undefined, noError = false) => {
    if (!radio) return;
    const newState = !radio.rx;

    window.api
      .setFrequencyState(
        radio.callsign,
        radio.frequency,
        newState,
        newState ? radio.tx : false,
        false,
        radio.onSpeaker,
        false,
        radio.isOutputMuted,
        radio.outputVolume
      )
      .then((ret) => {
        if (!ret && !noError) {
          reAddRadio(radio, 'RX');
          return;
        }
        setRadioState(radio.frequency, {
          rx: newState,
          tx: !newState ? false : radio.tx,
          xc: !false,
          crossCoupleAcross: false,
          onSpeaker: radio.onSpeaker,
          outputVolume: radio.outputVolume,
          isOutputMuted: radio.isOutputMuted
        });
      })
      .catch((err: unknown) => {
        console.error(err);
      });
  };

  const clickTx = (radio: RadioType | undefined, noError = false) => {
    if (!radio) return;
    const newState = !radio.tx;

    window.api
      .setFrequencyState(
        radio.callsign,
        radio.frequency,
        newState ? true : radio.rx, // If tx is true, rx must be true
        newState,
        false,
        radio.onSpeaker,
        false,
        radio.isOutputMuted,
        radio.outputVolume
      )
      .then((ret) => {
        if (!ret && !noError) {
          reAddRadio(radio, 'TX');
          return;
        }
        setRadioState(radio.frequency, {
          rx: !radio.rx && newState ? true : radio.rx,
          tx: newState,
          xc: false,
          crossCoupleAcross: false,
          onSpeaker: radio.onSpeaker,
          outputVolume: radio.outputVolume,
          isOutputMuted: radio.isOutputMuted
        });
      })
      .catch((err: unknown) => {
        console.error(err);
      });
  };

  const clickSpK = (radio: RadioType | undefined, noError = false) => {
    if (!radio) return;
    const newState = !radio.onSpeaker;
    window.api
      .setFrequencyState(
        radio.callsign,
        radio.frequency,
        radio.rx,
        radio.tx,
        radio.xc,
        newState,
        radio.crossCoupleAcross,
        radio.isOutputMuted,
        radio.outputVolume
      )
      .then((ret) => {
        if (!ret && !noError) {
          reAddRadio(radio, 'SPK');
          return;
        }
        setRadioState(radio.frequency, {
          rx: radio.rx,
          tx: radio.tx,
          xc: radio.xc,
          crossCoupleAcross: radio.crossCoupleAcross,
          onSpeaker: newState,
          outputVolume: radio.outputVolume,
          isOutputMuted: radio.isOutputMuted
        });
      })
      .catch((err: unknown) => {
        console.error(err);
      });
  };

  const toggleMute = (radio: RadioType | undefined) => {
    if (!radio) return;

    const newState = !radio.isOutputMuted;
    window.api
      .setFrequencyState(
        radio.callsign,
        radio.frequency,
        radio.rx,
        radio.tx,
        radio.xc,
        radio.onSpeaker,
        radio.crossCoupleAcross,
        newState,
        radio.outputVolume
      )
      .then((ret) => {
        if (!ret) {
          postError('Invalid action on invalid radio: Mute.');
          removeRadio(radio.frequency);
          return;
        }
        setRadioState(radio.frequency, {
          rx: radio.rx,
          tx: radio.tx,
          xc: radio.xc,
          crossCoupleAcross: radio.crossCoupleAcross,
          onSpeaker: radio.onSpeaker,
          outputVolume: radio.outputVolume,
          isOutputMuted: newState
        });
      })
      .catch((err: unknown) => {
        console.error(err);
      });
  };

  // In UnicomGuardBar.tsx, modify the useEffect:
  useEffect(() => {
    if (!isConnected) {
      void window.api.removeFrequency(UnicomFrequency).then((ret) => {
        if (!ret) return;
        removeRadio(UnicomFrequency);
      });
      void window.api.removeFrequency(GuardFrequency).then((ret) => {
        if (!ret) return;
        removeRadio(GuardFrequency);
      });
    } else if (!unicom || !guard) {
      const storedUnicomStationVolume = window.localStorage.getItem('UNICOMGUARDStationVolume');
      const stationUnicomVolumeToSet = storedUnicomStationVolume?.length
        ? parseInt(storedUnicomStationVolume)
        : 100;

      void window.api
        .addFrequency(UnicomFrequency, 'UNICOM', stationUnicomVolumeToSet)
        .then((ret) => {
          if (!ret) {
            console.error('Failed to add UNICOM frequency');
            return;
          }

          addRadio(UnicomFrequency, 'UNICOM', 'UNICOM');
        });

      // Setup GUARD with same volume as UNICOM
      void window.api
        .addFrequency(GuardFrequency, 'GUARD', stationUnicomVolumeToSet)
        .then(async (ret) => {
          if (!ret) {
            console.error('Failed to add GUARD frequency');
            return;
          }

          // Get the same stored volume as UNICOM
          const storedStationVolume = window.localStorage.getItem('UNICOMStationVolume');
          const stationVolumeToSet = storedStationVolume?.length
            ? parseInt(storedStationVolume)
            : 100;

          addRadio(GuardFrequency, 'GUARD', 'GUARD');
          setOutputVolume(GuardFrequency, stationVolumeToSet);
          await window.api.SetFrequencyRadioVolume(GuardFrequency, stationVolumeToSet);
        });
    }
  }, [isConnected]);

  useEffect(() => {
    if (!unicom) return;
    setLocalUnicomStationVolume(unicom.outputVolume);
  }, [unicom?.outputVolume]);

  useEffect(() => {
    if (!guard) return;
    setLocalUnicomStationVolume(guard.outputVolume);
  }, [guard?.outputVolume]);

  // Modify updateStationVolumeValue to use local state:
  const updateStationVolumeValue = async (newStationVolume: number) => {
    if (!unicom || !guard) return;

    // Update local state immediately for responsive UI
    setLocalUnicomStationVolume(newStationVolume);

    // Then update API
    try {
      await window.api.SetFrequencyRadioVolume(unicom.frequency, newStationVolume);
      await window.api.SetFrequencyRadioVolume(guard.frequency, newStationVolume);
      window.localStorage.setItem('UNICOMGUARDStationVolume', newStationVolume.toString());
    } catch (err) {
      // On error, revert to previous state
      setLocalUnicomStationVolume(unicom.outputVolume);
      console.error(err);
    }
  };

  const handleStationVolumeChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    void updateStationVolumeValue(event.target.valueAsNumber);
  };

  const handleStationVolumeMouseWheel = (event: React.WheelEvent<HTMLInputElement>) => {
    const newValue = Math.min(
      Math.max(localUnicomStationVolume + (event.deltaY > 0 ? -1 : 1), 0),
      100
    );
    void updateStationVolumeValue(newValue);
  };

  if (!showingUnicomBar) {
    return null;
  }

  return (
    <div className="unicom-bar-container">
      <span className="unicom-line-item">
        <span className="unicom-text " style={{ marginRight: '5px' }}>
          UNICOM
        </span>
        <button
          className={clsx(
            'btn sm-button h-full d-flex align-items-center justify-content-center',
            unicom?.isOutputMuted && 'btn-danger',
            !unicom?.rx && !unicom?.isOutputMuted && 'btn-info',
            unicom?.rx && !unicom.isOutputMuted && unicom.currentlyRx && 'btn-warning',
            unicom?.rx && !unicom.isOutputMuted && !unicom.currentlyRx && 'btn-success'
          )}
          disabled={!isConnected || !unicom}
          onClick={() => {
            clickRx(unicom);
          }}
          onContextMenu={() => {
            toggleMute(unicom);
          }}
        >
          {unicom?.isOutputMuted ? <VolumeX size={15} /> : 'RX'}
        </button>
        <button
          className={clsx(
            'btn sm-button',
            !unicom?.tx && 'btn-info',
            unicom?.tx && unicom.currentlyTx && 'btn-warning',
            unicom?.tx && !unicom.currentlyTx && 'btn-success'
          )}
          disabled={!isConnected || !unicom || !isAtc}
          onClick={() => {
            clickTx(unicom);
          }}
        >
          TX
        </button>
        <span className="hide-unicom-container">
          <button
            className={clsx(
              'btn sm-button',
              !unicom?.onSpeaker && 'btn-info',
              unicom?.onSpeaker && 'btn-success'
            )}
            disabled={!isConnected || !unicom}
            onClick={() => {
              clickSpK(unicom);
            }}
          >
            SPK
          </button>
        </span>
      </span>

      <span className="unicom-line-item">
        <span className="unicom-text" style={{ marginRight: '5px' }}>
          GUARD
        </span>
        <button
          className={clsx(
            'btn sm-button h-full d-flex align-items-center justify-content-center',
            guard?.isOutputMuted && 'btn-danger',
            !guard?.rx && !guard?.isOutputMuted && 'btn-info',
            guard?.rx && !guard.isOutputMuted && guard.currentlyRx && 'btn-warning',
            guard?.rx && !guard.isOutputMuted && !guard.currentlyRx && 'btn-success'
          )}
          disabled={!isConnected || !guard}
          onClick={() => {
            clickRx(guard);
          }}
          onContextMenu={() => {
            toggleMute(guard);
          }}
        >
          {guard?.isOutputMuted ? <VolumeX size={15} /> : 'RX'}
        </button>
        <button
          className={clsx(
            'btn sm-button',
            !guard?.tx && 'btn-info',
            guard?.tx && guard.currentlyTx && 'btn-warning',
            guard?.tx && !guard.currentlyTx && 'btn-success'
          )}
          disabled={!isConnected || !guard || !isAtc}
          onClick={() => {
            clickTx(guard);
          }}
        >
          TX
        </button>
        <span className="hide-unicom-container">
          <button
            className={clsx(
              'btn sm-button',
              !guard?.onSpeaker && 'btn-info',
              guard?.onSpeaker && 'btn-success'
            )}
            disabled={!isConnected || !guard}
            onClick={() => {
              clickSpK(guard);
            }}
          >
            SPK
          </button>
        </span>
      </span>
      <span
        className="hide-unicom-container"
        style={{
          lineHeight: '30px'
        }}
      >
        {!isReducedSize && (
          <span className="unicom-text " style={{ marginRight: '10px', lineHeight: '29px' }}>
            VOLUME
          </span>
        )}
        <input
          type="range"
          className="form-range unicom-text unicom-volume-bar "
          style={{
            lineHeight: '30px'
          }}
          min="0"
          max="100"
          step="1"
          value={localUnicomStationVolume}
          onChange={handleStationVolumeChange}
          onWheel={handleStationVolumeMouseWheel}
        ></input>
      </span>
    </div>
  );
};

export default UnicomGuardBar;
