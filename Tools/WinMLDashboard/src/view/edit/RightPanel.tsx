import { Label } from 'office-ui-fabric-react/lib/Label';
import * as React from 'react';
import { connect } from 'react-redux';

import Collapsible from '../../components/Collapsible';
import KeyValueEditor from '../../components/KeyValueEditor'
import Resizable from '../../components/Resizable';
import { setMetadataProps } from '../../datastore/actionCreators';
import IState, { IMetadataProps } from '../../datastore/state';
import MetadataSchema from '../../schema/Metadata';

import './Panel.css';

interface IComponentProperties {
    // Redux properties
    nodes: any,
    metadataProps: IMetadataProps,
    setMetadataProps: typeof setMetadataProps,
    showRight: boolean,
}

class RightPanel extends React.Component<IComponentProperties, {}> {
    constructor(props: any) {
        super(props);
    }
    public render() {

        return (
            <div className="Unselectable">
                <Resizable isRightPanel={true} visible={this.props.showRight}>
                    <Label className="toggleLabel">Model</Label>
                    <div className='Panel'>
                        <Collapsible label='Properties'>
                            <KeyValueEditor getState={this.getPropertiesFromState} schema={{ type: 'object' }} />
                        </Collapsible>
                        <Collapsible label='Metadata properties'>
                            <KeyValueEditor actionCreator={setMetadataProps} getState={this.getMetadataPropsFromState} schema={MetadataSchema} />
                        </Collapsible>
                    </div>
                </Resizable>
            </div>
        );
    }
    private getMetadataPropsFromState = (state: IState) => state.metadataProps;
    private getPropertiesFromState = (state: IState) => state.properties;
}

const mapStateToProps = (state: IState) => ({
    metadataProps: state.metadataProps,
    nodes: state.nodes,
    properties: state.properties,
    showRight: state.showRight,
});

const mapDispatchToProps = {
    setMetadataProps,
}

export default connect(mapStateToProps, mapDispatchToProps)(RightPanel);
